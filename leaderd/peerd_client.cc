// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/peerd_client.h"

#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

namespace leaderd {
namespace {
const char kGroupFieldFormat[] = "group_%" PRIuS;
const char kGroupFieldPrefix[] = "group_";
const char kPeerdPeerSelfPath[] = "/org/chromium/peerd/Self";
const char kServiceName[] = "privet-ldrsp";
}  // namespace

using org::chromium::peerd::ServiceProxyInterface;

void PeerdClient::SetDelegate(Delegate* delegate) { delegate_ = delegate; }

std::set<std::string> PeerdClient::GetPeersMatchingGroup(
    const std::string& in_group_id) {
  std::set<std::string> peers;
  for (const auto& uuid_path_pair : uuids_to_paths_) {
    const ServiceProxyInterface* service_proxy =
        GetServiceProxy(uuid_path_pair.second);
    if (!service_proxy) {
      continue;
    }
    // Take all the group fields out of the text record.
    std::map<std::string, std::string> service_info =
        service_proxy->service_info();
    for (const auto& field : service_info) {
      if (StartsWithASCII(field.first, kGroupFieldPrefix, true) &&
          field.second == in_group_id) {
        peers.insert(uuid_path_pair.first);
        break;
      }
    }
  }
  return peers;
}

void PeerdClient::StartMonitoring() {
  if (monitoring_) {
    return;
  }
  monitoring_ = true;
  chromeos::ErrorPtr error;
  org::chromium::peerd::ManagerProxyInterface* proxy = GetManagerProxy();
  if (proxy && !proxy->StartMonitoring({"mDNS"}, {}, &monitor_token_, &error)) {
    LOG(ERROR) << "StartMonitoring failed:" << error->GetMessage();
  }
}

void PeerdClient::StopMonitoring() {
  monitoring_ = false;
  if (monitor_token_.empty()) {
    return;
  }

  chromeos::ErrorPtr error;
  org::chromium::peerd::ManagerProxyInterface* proxy = GetManagerProxy();
  if (proxy && !proxy->StopMonitoring(monitor_token_, &error)) {
    LOG(ERROR) << "StopMonitoring failed:" << error->GetMessage();
  }
}

std::vector<std::tuple<std::vector<uint8_t>, uint16_t>> PeerdClient::GetIPInfo(
    const std::string& peer_uuid) {
  std::vector<std::tuple<std::vector<uint8_t>, uint16_t>> addresses;

  const auto& uuid_to_path = uuids_to_paths_.find(peer_uuid);
  if (uuid_to_path == uuids_to_paths_.end()) {
    return addresses;
  }

  const org::chromium::peerd::ServiceProxyInterface* service =
      GetServiceProxy(uuid_to_path->second);
  if (service) {
    addresses = service->ip_infos();
  }
  return addresses;
}

void PeerdClient::UpdatePeerService(
    ServiceProxyInterface* service_proxy,
    const dbus::ObjectPath& service_object_path) {
  if (StartsWithASCII(service_object_path.value(), kPeerdPeerSelfPath, true)) {
    VLOG(3) << "Ignoring service discovered on ourselves.";
    return;
  }
  VLOG(1) << "Found peer with id=" << service_proxy->peer_id();
  uuids_to_paths_[service_proxy->peer_id()] = service_object_path;

  // Take all the group fields out of the text record.
  std::map<std::string, std::string> service_info =
      service_proxy->service_info();
  std::set<std::string> groups;
  for (const auto& txt_field_pair : service_info) {
    if (StartsWithASCII(txt_field_pair.first, kGroupFieldPrefix, true)) {
      groups.insert(txt_field_pair.second);
    }
  }

  delegate_->OnPeerGroupsChanged(service_proxy->peer_id(), groups);
}

void PeerdClient::RemovePeerService(const dbus::ObjectPath& object_path) {
  ServiceProxyInterface* service = GetServiceProxy(object_path);
  DCHECK(service) << "Failed to retrieve ServiceProxy instance.";
  const auto it = uuids_to_paths_.find(service->peer_id());
  if (it == uuids_to_paths_.end()) {
    return;  // Must have been a service we don't care about.
  }
  delegate_->OnPeerGroupsChanged(service->peer_id(), {});
  uuids_to_paths_.erase(service->peer_id());
}

void PeerdClient::PublishGroups(uint16_t port,
                                const std::vector<std::string>& groups) {
  org::chromium::peerd::ManagerProxyInterface* proxy = GetManagerProxy();
  if (!proxy) {
    return;
  }

  std::map<std::string, chromeos::Any> mdns_options{
      {"port", chromeos::Any{port}},
  };

  std::map<std::string, std::string> txt_record{
      {"leaderd_ver", "1.0"},
  };

  size_t group_index = 0;
  for (const auto& group : groups) {
    std::string field = base::StringPrintf(kGroupFieldFormat, ++group_index);
    txt_record.emplace(field, group);
  }

  chromeos::ErrorPtr error;
  if (!proxy->ExposeService(kServiceName, txt_record, {{"mdns", mdns_options}},
                            &error)) {
    LOG(ERROR) << "ExposeService failed:" << error->GetMessage();
  }
}

PeerdClientImpl::PeerdClientImpl(const scoped_refptr<dbus::Bus>& bus,
                                 const std::string& peerd_service_name)
    : peerd_object_manager_proxy_{bus, peerd_service_name} {
  peerd_object_manager_proxy_.SetManagerAddedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdManagerAdded, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetManagerRemovedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdManagerRemoved, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetPeerAddedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdPeerAdded, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetServiceAddedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdServiceAdded, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetServiceRemovedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdServiceRemoved, weak_ptr_factory_.GetWeakPtr()));
}

void PeerdClientImpl::OnPeerdManagerAdded(
    org::chromium::peerd::ManagerProxy* manager_proxy) {
  VLOG(1) << "peerd manager online.";
  if (monitoring_) {
    // StartMonitoring will adjust it back; setting it false forces us to
    // reconnect to it.
    monitoring_ = false;
    StartMonitoring();
  }
  delegate_->OnPeerdAvailable();
}

void PeerdClientImpl::OnPeerdManagerRemoved(
    const dbus::ObjectPath& object_path) {
  VLOG(1) << "peerd manager offline.";
  delegate_->OnPeerdDeath();
  monitor_token_.clear();
}

void PeerdClientImpl::OnPeerdPeerAdded(
    org::chromium::peerd::PeerProxy* peer_proxy) {
  if (peer_proxy->GetObjectPath().value() == kPeerdPeerSelfPath) {
    delegate_->OnSelfIdChanged(peer_proxy->uuid());
  }
}

void PeerdClientImpl::OnPeerdServiceAdded(
    org::chromium::peerd::ServiceProxy* service_proxy) {
  if (service_proxy->service_id() != kServiceName) return;
  UpdatePeerService(service_proxy, service_proxy->GetObjectPath());
  service_proxy->SetPropertyChangedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdServiceChanged, weak_ptr_factory_.GetWeakPtr()));
}

void PeerdClientImpl::OnPeerdServiceRemoved(
    const dbus::ObjectPath& object_path) {
  RemovePeerService(object_path);
}

void PeerdClientImpl::OnPeerdServiceChanged(
    org::chromium::peerd::ServiceProxy* service_proxy,
    const std::string& property) {
  UpdatePeerService(service_proxy, service_proxy->GetObjectPath());
}

org::chromium::peerd::PeerProxyInterface* PeerdClientImpl::GetPeerProxy(
    const dbus::ObjectPath& object_path) {
  return peerd_object_manager_proxy_.GetPeerProxy(object_path);
}

org::chromium::peerd::ServiceProxyInterface* PeerdClientImpl::GetServiceProxy(
    const dbus::ObjectPath& object_path) {
  return peerd_object_manager_proxy_.GetServiceProxy(object_path);
}

org::chromium::peerd::ManagerProxyInterface*
PeerdClientImpl::GetManagerProxy() {
  return peerd_object_manager_proxy_.GetManagerProxy();
}

}  // namespace leaderd
