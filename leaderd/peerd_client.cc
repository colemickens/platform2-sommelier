// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/peerd_client.h"

#include <base/logging.h>
#include <base/strings/string_util.h>

namespace leaderd {

namespace {
const char kGroupFieldPrefix[] = "group_";
const char kPeerdPeerSelfPath[] = "/org/chromium/peerd/Self";
const char kServiceName[] = "privet-ldrsp";
const char kServicesSubPath[] = "/services/";
}  // namespace

void PeerdClient::SetDelegate(Delegate* delegate) { delegate_ = delegate; }

std::set<std::string> PeerdClient::GetPeersMatchingGroup(
    const std::string& in_group_id) {
  std::set<std::string> peers;
  for (const auto& path_uuid_pair : paths_to_uuids_) {
    const org::chromium::peerd::ServiceProxyInterface* service_proxy =
        GetServiceProxy(path_uuid_pair.first);
    if (service_proxy) {
      // Take all the group fields out of the text record.
      std::map<std::string, std::string> service_info =
          service_proxy->service_info();
      for (const auto& field : service_info) {
        if (StartsWithASCII(field.first, kGroupFieldPrefix, true) &&
            field.second == in_group_id) {
          peers.insert(path_uuid_pair.second);
          break;
        }
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
    org::chromium::peerd::ServiceProxyInterface* service_proxy,
    const dbus::ObjectPath& object_path) {
  if (service_proxy->service_id() != kServiceName) return;

  const std::string& service_path = object_path.value();
  std::string peer_path =
      service_path.substr(0, service_path.find(kServicesSubPath));
  const org::chromium::peerd::PeerProxyInterface* peer =
      GetPeerProxy(dbus::ObjectPath(peer_path));

  if (!peer) return;

  paths_to_uuids_[object_path] = peer->uuid();
  uuids_to_paths_[peer->uuid()] = object_path;

  // Take all the group fields out of the text record.
  std::map<std::string, std::string> service_info =
      service_proxy->service_info();
  std::set<std::string> groups;
  for (const auto& txt_field_pair : service_info) {
    if (StartsWithASCII(txt_field_pair.first, kGroupFieldPrefix, true)) {
      groups.insert(txt_field_pair.second);
    }
  }

  delegate_->OnPeerGroupsChanged(peer->uuid(), groups);
}

void PeerdClient::RemovePeerService(const dbus::ObjectPath& object_path) {
  const auto& path_to_uuid = paths_to_uuids_.find(object_path);
  if (path_to_uuid == paths_to_uuids_.end()) {
    return;
  }
  delegate_->OnPeerGroupsChanged(path_to_uuid->second, {});
  uuids_to_paths_.erase(path_to_uuid->second);
  paths_to_uuids_.erase(path_to_uuid);
}

PeerdClientImpl::PeerdClientImpl(const scoped_refptr<dbus::Bus>& bus)
    : peerd_object_manager_proxy_{bus} {
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
