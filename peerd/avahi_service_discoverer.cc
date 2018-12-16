// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_service_discoverer.h"

#include <arpa/inet.h>
#include <avahi-common/address.h>
#include <avahi-common/defs.h>

#include <utility>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <brillo/dbus/dbus_signal_handler.h>
#include <brillo/strings/string_utils.h>
#include <dbus/object_proxy.h>

#include "peerd/avahi_client.h"
#include "peerd/constants.h"
#include "peerd/dbus_constants.h"
#include "peerd/peer_manager_interface.h"
#include "peerd/service.h"

using brillo::dbus_utils::AsyncEventSequencer;
using brillo::dbus_utils::CallMethodAndBlock;
using brillo::dbus_utils::ConnectToSignal;
using brillo::dbus_utils::ExtractMethodCallResults;
using brillo::string_utils::Join;
using dbus::ObjectPath;
using dbus::ObjectProxy;
using peerd::dbus_constants::avahi::kServiceBrowserInterface;
using peerd::dbus_constants::avahi::kServiceBrowserMethodFree;
using peerd::dbus_constants::avahi::kServiceBrowserSignalFailure;
using peerd::dbus_constants::avahi::kServiceBrowserSignalItemNew;
using peerd::dbus_constants::avahi::kServiceBrowserSignalItemRemove;
using peerd::dbus_constants::avahi::kServiceName;
using peerd::dbus_constants::avahi::kServiceResolverInterface;
using peerd::dbus_constants::avahi::kServiceResolverMethodFree;
using peerd::dbus_constants::avahi::kServiceResolverSignalFound;
using peerd::dbus_constants::avahi::kServiceResolverSignalFailure;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace peerd {

AvahiServiceDiscoverer::AvahiServiceDiscoverer(
    const scoped_refptr<dbus::Bus>& bus,
    ObjectProxy* avahi_proxy,
    PeerManagerInterface* peer_manager) : bus_(bus),
                                          avahi_proxy_(avahi_proxy),
                                          peer_manager_(peer_manager),
                                          protocol_(AVAHI_PROTO_INET) {
}

AvahiServiceDiscoverer::~AvahiServiceDiscoverer() {
  if (serbus_browser_) {
    auto resp = CallMethodAndBlock(serbus_browser_, kServiceBrowserInterface,
                                   kServiceBrowserMethodFree, nullptr);
    if (resp) { ExtractMethodCallResults(resp.get(), nullptr); }
    serbus_browser_->Detach();
    serbus_browser_ = nullptr;
  }
}

void AvahiServiceDiscoverer::RegisterAsync(
    const CompletionAction& completion_callback) {
  serbus_browser_ = BrowseServices(
      constants::mdns::kSerbusServiceType, completion_callback);
}

bool AvahiServiceDiscoverer::txt_list2service_info(const TxtList& txt_list,
                                                   Service::ServiceInfo* info) {
  for (const vector<uint8_t>& label : txt_list) {
    string label_str{label.cbegin(), label.cend()};
    string key, value;
    brillo::string_utils::SplitAtFirst(label_str, "=", &key, &value, false);
    info->emplace(key, value);
  }
  return Service::IsValidServiceInfo(nullptr, *info);
}

ObjectProxy* AvahiServiceDiscoverer::BrowseServices(
    const string& service_type,
    const CompletionAction& cb) {
  const uint32_t flags{0};
  auto resp = CallMethodAndBlock(
      avahi_proxy_, dbus_constants::avahi::kServerInterface,
      dbus_constants::avahi::kServerMethodServiceBrowserNew,
      nullptr,
      avahi_if_t{AVAHI_IF_UNSPEC},  // Look across all interfaces.
      protocol_,  // Only use our chosen protocol.
      service_type,
      string{},  // Empty domain indicates default.
      flags);
  ObjectPath path;
  if (!resp || !ExtractMethodCallResults(resp.get(), nullptr, &path)) {
    LOG(ERROR) << "Failed to create service browser, not monitoring mDNS.";
    base::Closure failure_cb = base::Bind(
        [](const CompletionAction& action) { action.Run(false); }, cb);
    base::MessageLoop::current()->task_runner()->PostTask(FROM_HERE,
                                                          failure_cb);
    return nullptr;
  }
  ObjectProxy* result = bus_->GetObjectProxy(kServiceName, path);
  CHECK(result);  // Makes unittests fail more gracefully.
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  ConnectToSignal(result,
                  kServiceBrowserInterface, kServiceBrowserSignalItemNew,
                  base::Bind(&AvahiServiceDiscoverer::HandleItemNew,
                             weak_ptr_factory_.GetWeakPtr()),
                  sequencer->GetExportHandler(
                      kServiceBrowserInterface, kServiceBrowserSignalItemNew,
                      "Failed to connect to Avahi ItemNew signal for "
                      "service type=" + service_type,
                      false));
  ConnectToSignal(result,
                  kServiceBrowserInterface, kServiceBrowserSignalItemRemove,
                  base::Bind(&AvahiServiceDiscoverer::HandleItemRemove,
                             weak_ptr_factory_.GetWeakPtr()),
                  sequencer->GetExportHandler(
                      kServiceBrowserInterface, kServiceBrowserSignalItemRemove,
                      "Failed to connect to Avahi ItemRemove signal for "
                      "service type=" + service_type,
                      false));
  // No idea why we would get this, but lets register for it anyway.
  ConnectToSignal(result,
                  kServiceBrowserInterface, kServiceBrowserSignalFailure,
                  base::Bind(&AvahiServiceDiscoverer::HandleFailure,
                             weak_ptr_factory_.GetWeakPtr(),
                             service_type),
                  sequencer->GetExportHandler(
                      kServiceBrowserInterface, kServiceBrowserSignalFailure,
                      "Failed to connect to Avahi Failure signal for "
                      "service type=" + service_type,
                      false));
  sequencer->OnAllTasksCompletedCall({cb});
  return result;
}

void AvahiServiceDiscoverer::OnPeerServicesChanged(
    const std::string& peer_id,
    set<string> new_service_types) {
  // First, update all existing browsers.
  auto it = peers_for_service_.begin();
  while (it != peers_for_service_.end()) {
    const string& service_type = it->first;
    set<string>& peer_ids_for_service = it->second;
    const bool was_relevant = peer_ids_for_service.count(peer_id) > 0;
    const bool is_relevant = new_service_types.count(service_type) > 0;
    if (was_relevant && is_relevant) {
      // We were already browsing for this service.  No action item here.
      VLOG(2) << "Peer=" << peer_id << " continues to be interested in "
                 "service type=" << service_type;
      new_service_types.erase(service_type);
    } else if (was_relevant && !is_relevant) {
      VLOG(2) << "Service type=" << service_type << " has been removed "
              << "for peer=" << peer_id;
      // This peer was advertising this service type, but now isn't.
      // Remove this peer from the list of relevant parties.
      peer_ids_for_service.erase(peer_id);
      if (peer_ids_for_service.empty()) {
        VLOG(2) << "No peers interested in service type=" << service_type
                << ". Removing browser.";
        // No one seems to advertise this service any more.
        // Remove the browser for the service.
        auto browser_it = browsers_.find(service_type);
        CHECK(browser_it != browsers_.end());
        dbus::ObjectProxy* browser = browser_it->second;
        browsers_.erase(browser_it);
        auto resp = CallMethodAndBlock(browser, kServiceBrowserInterface,
                                       kServiceBrowserMethodFree, nullptr);
        if (resp) { ExtractMethodCallResults(resp.get(), nullptr); }
        browser->Detach();
        browser = nullptr;
        // And remove the list of relevant peers.
        it = peers_for_service_.erase(it);
        continue;
      }
    } else if (!was_relevant && is_relevant) {
      // Someone else advertised this service, and this peer does as well now.
      // Add new peer to the list of relevant peers.
      VLOG(2) << "Reusing existing browser for service type=" << service_type
              << " for peer=" << peer_id;
      peer_ids_for_service.insert(peer_id);
      new_service_types.erase(service_type);
    }
    ++it;
  }
  // Second, there may be new service types this peer advertises.
  // Browse for them.
  for (const string& service_type : new_service_types) {
    VLOG(2) << "Adding new service browser for type=" << service_type;
    peers_for_service_[service_type].insert(peer_id);
    browsers_[service_type] = BrowseServices(
        service_type, AsyncEventSequencer::GetDefaultCompletionAction());
  }
}

void AvahiServiceDiscoverer::OnPeerServicesRemoved(const string& peer_id) {
  OnPeerServicesChanged(peer_id, {});
}

void AvahiServiceDiscoverer::RegisterResolver(avahi_if_t interface,
                                              const string& name,
                                              const string& type,
                                              const string& domain) {
  const uint32_t resolver_flags{0};
  // Specify that we want to send queries and receive records over |protocol_|
  // (i.e. don't discover IPv6 services over IPv4 or vice versa.
  auto resp = CallMethodAndBlock(
      avahi_proxy_, dbus_constants::avahi::kServerInterface,
      dbus_constants::avahi::kServerMethodServiceResolverNew,
      nullptr, interface, protocol_, name, type, domain, protocol_,
      resolver_flags);
  ObjectPath path;
  if (!resp || !ExtractMethodCallResults(resp.get(), nullptr, &path)) {
    LOG(ERROR) << "Failed to create service resolver for service " << type;
    return;
  }
  ObjectProxy* resolver = bus_->GetObjectProxy(kServiceName, path);
  CHECK(resolver);  // Makes unittests fail in more obvious ways.
  resolvers_[type][resolv_key_t{interface, name, domain}] = resolver;
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  resolver->ConnectToSignal(
      kServiceResolverInterface,
      kServiceResolverSignalFound,
      base::Bind(&AvahiServiceDiscoverer::HandleFound,
                 weak_ptr_factory_.GetWeakPtr()),
      sequencer->GetExportHandler(
          kServiceResolverInterface,
          kServiceResolverSignalFound,
          "Failed to connect to Avahi Found signal for resolver.",
          false));
  // No idea why we would get this, but lets register for it anyway.
  resolver->ConnectToSignal(
      kServiceResolverInterface,
      kServiceResolverSignalFailure,
      base::Bind(&AvahiServiceDiscoverer::HandleResolverFailure,
                 weak_ptr_factory_.GetWeakPtr(),
                 interface, name, type, domain),
      sequencer->GetExportHandler(
          kServiceResolverInterface,
          kServiceResolverSignalFailure,
          "Failed to connect to Avahi Failure signal for Resolver.",
          false));
  sequencer->OnAllTasksCompletedCall({});
}

void AvahiServiceDiscoverer::RemoveResolver(avahi_if_t interface,
                                            const std::string& name,
                                            const std::string& type,
                                            const std::string& domain) {
  auto type_it = resolvers_.find(type);
  if (type_it == resolvers_.end()) {
    LOG(ERROR) << "Tried to remove Resolver for unknown type: " << type;
    return;
  }
  ResolversForType& type_resolvers = type_it->second;
  resolv_key_t resolver_key{interface, name, domain};
  auto it = type_resolvers.find(resolver_key);
  if (it == type_resolvers.end()) {
    LOG(ERROR) << "Tried to remove unknown Resolver for type=" << type
               << ", <" << interface << ", " << name << ", " << domain << ">.";
    return;
  }
  auto resp = CallMethodAndBlock(it->second, kServiceResolverInterface,
                                 kServiceResolverMethodFree, nullptr);
  if (resp) { ExtractMethodCallResults(resp.get(), nullptr); }
  it->second->Detach();
  type_resolvers.erase(it);
  if (type_resolvers.empty()) {
    resolvers_.erase(type_it);
  }
}

void AvahiServiceDiscoverer::HandleItemNew(avahi_if_t interface,
                                           avahi_proto_t protocol,
                                           const string& name,
                                           const string& type,
                                           const string& domain,
                                           uint32_t flags) {
  VLOG(1) << "Discovered service: " << interface << ", " << protocol << ", "
          << name << ", " << type << ", " << domain << ", " << flags;
  if ((flags & AVAHI_LOOKUP_RESULT_LOCAL) != 0) {
    VLOG(1) << "Ignoring local service.";
    return;
  }
  CHECK(protocol == protocol_);
  RegisterResolver(interface, name, type, domain);
}

void AvahiServiceDiscoverer::HandleItemRemove(avahi_if_t interface,
                                              avahi_proto_t protocol,
                                              const string& name,
                                              const string& type,
                                              const string& domain,
                                              uint32_t flags) {
  VLOG(1) << "Removed service: " << interface << ", " << protocol << ", "
          << name << ", " << type << ", " << domain << ", " << flags;
  RemoveResolver(interface, name, type, domain);
  if (type == constants::mdns::kSerbusServiceType) {
    auto it = serbus_record_to_peer_id_.find(
        resolv_key_t{interface, name, domain});
    if (it == serbus_record_to_peer_id_.end()) {
      LOG(ERROR) << "Peer with unknown peer id has gone away.";
      return;
    }
    OnPeerServicesRemoved(it->second);
    serbus_record_to_peer_id_.erase(it);
  }
}

void AvahiServiceDiscoverer::HandleFailure(const string& service_type,
                                           const string& message) {
  // TODO(wiley) What should we do here?
  LOG(ERROR) << "Avahi ServiceDiscoverer in failure state for service type="
             << service_type << " : " << message;
}

void AvahiServiceDiscoverer::HandleFound(dbus::Signal* signal) {
  VLOG(1) << "HandleFound called to handle signal from Resolver.";
  avahi_if_t interface;
  avahi_proto_t protocol;
  string name, type, domain, host, address;
  // We could for instance, conceivably discover IPv6 records over
  // and IPv6 interface.
  avahi_proto_t application_protocol;
  uint16_t port;
  TxtList txt_list;
  uint32_t flags;
  const bool parse_success = ExtractMethodCallResults(
      signal, nullptr, &interface, &protocol, &name, &type, &domain, &host,
      &application_protocol, &address, &port, &txt_list, &flags);
  if (!parse_success) {
    LOG(ERROR) << "Failed parsing Found signal from resolver.";
    return;
  }
  Service::ServiceInfo info;
  if (!txt_list2service_info(txt_list, &info)) {
    LOG(ERROR) << "Ignoring invalid serbus mDNS record.";
    return;
  }
  base::Time last_seen = base::Time::Now();
  if (type == constants::mdns::kSerbusServiceType) {
    VLOG(1) << "Found serbus TXT record update.";
    if (info.size() != 3) {
      LOG(ERROR) << "Peer is advertising serbus record with incorrect number "
                    "of fields: " << info.size();
      return;
    }
    auto it_peer_id = info.find(constants::mdns::kSerbusPeerId);
    auto it_version = info.find(constants::mdns::kSerbusVersion);
    auto it_services = info.find(constants::mdns::kSerbusServiceList);
    if (it_peer_id == info.end()) {
      LOG(ERROR) << "Ignoring Peer with missing peer id.";
      return;
    }
    if (it_version == info.end()) {
      LOG(ERROR) << "Ignoring Peer with missing version string.";
      return;
    }
    if (it_services == info.end()) {
      LOG(ERROR) << "Ignoring Peer with missing services list.";
      return;
    }
    vector<string> service_ids{
        brillo::string_utils::Split(it_services->second,
                                    constants::mdns::kSerbusServiceDelimiter,
                                    false,
                                    false)};
    set<string> service_types{};
    for (string& service : service_ids) {
      // Validate the service id.
      if (!Service::IsValidServiceId(nullptr, service)) {
        LOG(ERROR) << "Ignoring Peer with invalid Serbus record.";
        return;
      }
      if (service == constants::kSerbusServiceId) {
        LOG(ERROR) << "Ignoring Peer advertising Serbus in Serbus record.";
        return;
      }
      service_types.insert(AvahiClient::GetServiceType(service));
    }
    // TODO(wiley) We don't actually do anything with the version
    const string& peer_id = it_peer_id->second;
    peer_manager_->OnPeerDiscovered(
        peer_id, last_seen, technologies::kMDNS);
    const resolv_key_t serbus_key{interface, name, domain};
    auto pair = serbus_record_to_peer_id_.emplace(serbus_key, peer_id);
    if (!pair.second) {
      LOG(WARNING) << "Peer id has changed for remote mDNS peer.";
      OnPeerServicesRemoved(pair.first->second);
      pair.first->second = peer_id;
    }
    OnPeerServicesChanged(peer_id, std::move(service_types));
  } else {
    VLOG(1) << "Found Serbus service record update.";
    // Assume that the same name is used for the peer's serbus and service
    // records.
    const resolv_key_t serbus_key{interface, name, domain};
    auto it = serbus_record_to_peer_id_.find(serbus_key);
    if (it == serbus_record_to_peer_id_.end()) {
      LOG(ERROR) << "Found service for unknown peer.";
      return;
    }
    CHECK(protocol == protocol_) << "Resolved record for unexpected protocol.";
    vector<uint8_t> addr_bytes;
    if (protocol == AVAHI_PROTO_INET) {
      in_addr raw_addr;
      if (inet_pton(AF_INET, address.c_str(), &raw_addr) != 1) {
        LOG(ERROR) << "Failed to parse IPv4 address.";
        return;
      }
      auto ptr = reinterpret_cast<const uint8_t*>(&raw_addr.s_addr);
      addr_bytes.assign(ptr, ptr + sizeof(raw_addr.s_addr));
    } else if (protocol == AVAHI_PROTO_INET6) {
      in6_addr raw_addr;
      if (inet_pton(AF_INET6, address.c_str(), &raw_addr) != 1) {
        LOG(ERROR) << "Failed to parse IPv6 address.";
        return;
      }
      auto ptr = reinterpret_cast<const uint8_t*>(&raw_addr.s6_addr);
      addr_bytes.assign(ptr, ptr + sizeof(raw_addr.s6_addr));
    } else {
      LOG(ERROR) << "Received mDNS records over a protocol other than IPv4/6 ("
                 << protocol << ")?";
      return;
    }
    peer_manager_->OnServiceDiscovered(
        it->second, AvahiClient::GetServiceId(type),
        info, {Service::IpAddress{addr_bytes, port}},
        last_seen, technologies::kMDNS);
  }
}

void AvahiServiceDiscoverer::HandleResolverFailure(avahi_if_t interface,
                                                   const std::string& name,
                                                   const std::string& type,
                                                   const std::string& domain,
                                                   dbus::Signal* signal) {
  string message;
  ExtractMethodCallResults(signal, nullptr, &message);
  LOG(ERROR) << "Resolver for type=" << type
             << ", <" << interface << ", " << name << ", " << domain << ">"
             << " reports failure: " << message;
  RemoveResolver(interface, name, type, domain);
}

}  // namespace peerd
