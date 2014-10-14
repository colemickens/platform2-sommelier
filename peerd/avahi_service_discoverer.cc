// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_service_discoverer.h"

#include <string>

#include <avahi-common/address.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <dbus/object_proxy.h>

#include "peerd/avahi_client.h"
#include "peerd/constants.h"
#include "peerd/dbus_constants.h"
#include "peerd/service.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::CallMethodAndBlock;
using chromeos::dbus_utils::ConnectToSignal;
using chromeos::dbus_utils::ExtractMethodCallResults;
using dbus::ObjectPath;
using dbus::ObjectProxy;
using peerd::dbus_constants::avahi::kServiceBrowserInterface;
using peerd::dbus_constants::avahi::kServiceBrowserMethodFree;
using peerd::dbus_constants::avahi::kServiceBrowserSignalFailure;
using peerd::dbus_constants::avahi::kServiceBrowserSignalItemNew;
using peerd::dbus_constants::avahi::kServiceBrowserSignalItemRemove;
using peerd::dbus_constants::avahi::kServiceName;
using std::string;

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
    serbus_browser_->Detach();
    serbus_browser_ = nullptr;
  }
}

void AvahiServiceDiscoverer::RegisterAsync(
    const CompletionAction& completion_callback) {
  serbus_browser_ = BrowseServices(
      constants::mdns::kSerbusServiceType, completion_callback);
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
    base::Closure failure_cb = base::Bind([cb]() { cb.Run(false); });
    base::MessageLoop::current()->PostTask(FROM_HERE, failure_cb);
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

void AvahiServiceDiscoverer::HandleItemNew(avahi_if_t interface,
                                           avahi_proto_t protocol,
                                           const string& name,
                                           const string& type,
                                           const string& domain,
                                           uint32_t flags) {
  // TODO(wiley) Resolve the TXT record and report this peer to the PeerManager.
  LOG(INFO) << "Discovered service: "
            << interface << ", "
            << protocol << ", "
            << name << ", "
            << type << ", "
            << domain << ", "
            << flags;
}

void AvahiServiceDiscoverer::HandleItemRemove(avahi_if_t interface,
                                              avahi_proto_t protocol,
                                              const string& name,
                                              const string& type,
                                              const string& domain,
                                              uint32_t flags) {
  // TODO(wiley) Remove this service from the PeerManager.
  LOG(INFO) << "Removed service: "
            << interface << ", "
            << protocol << ", "
            << name << ", "
            << type << ", "
            << domain << ", "
            << flags;
}

void AvahiServiceDiscoverer::HandleFailure(const string& service_type,
                                           const string& message) {
  // TODO(wiley) What should we do here?
  LOG(ERROR) << "Avahi ServiceDiscoverer in failure state for service type="
             << service_type << " : " << message;
}

}  // namespace peerd
