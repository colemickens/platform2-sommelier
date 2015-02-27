// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/peerd_client.h"

#include <base/logging.h>

namespace leaderd {

void PeerdClient::SetDelegate(Delegate* delegate) { delegate_ = delegate; }

PeerdClientImpl::PeerdClientImpl(const scoped_refptr<dbus::Bus>& bus)
    : peerd_object_manager_proxy_{bus} {
  peerd_object_manager_proxy_.SetManagerAddedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdManagerAdded, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetManagerRemovedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdManagerRemoved, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetPeerAddedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdPeerAdded, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetPeerRemovedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdPeerRemoved, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetServiceAddedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdServiceAdded, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetServiceRemovedCallback(base::Bind(
      &PeerdClientImpl::OnPeerdServiceRemoved, weak_ptr_factory_.GetWeakPtr()));
}

org::chromium::peerd::ManagerProxyInterface*
PeerdClientImpl::GetManagerProxy() {
  return peerd_object_manager_proxy_.GetManagerProxy();
}

org::chromium::peerd::PeerProxyInterface* PeerdClientImpl::GetPeerProxy(
    const dbus::ObjectPath& object_path) {
  return peerd_object_manager_proxy_.GetPeerProxy(object_path);
}

org::chromium::peerd::ServiceProxyInterface* PeerdClientImpl::GetServiceProxy(
    const dbus::ObjectPath& object_path) {
  return peerd_object_manager_proxy_.GetServiceProxy(object_path);
}

void PeerdClientImpl::OnPeerdManagerAdded(
    org::chromium::peerd::ManagerProxy* manager_proxy) {
  delegate_->OnPeerdManagerAdded(manager_proxy);
}

void PeerdClientImpl::OnPeerdManagerRemoved(
    const dbus::ObjectPath& object_path) {
  delegate_->OnPeerdManagerRemoved(object_path);
}

void PeerdClientImpl::OnPeerdPeerAdded(
    org::chromium::peerd::PeerProxy* peer_proxy) {
  delegate_->OnPeerdPeerAdded(peer_proxy, peer_proxy->GetObjectPath());
}

void PeerdClientImpl::OnPeerdPeerRemoved(const dbus::ObjectPath& object_path) {
  delegate_->OnPeerdPeerRemoved(object_path);
}

void PeerdClientImpl::OnPeerdServiceAdded(
    org::chromium::peerd::ServiceProxy* service_proxy) {
  delegate_->OnPeerdServiceAdded(service_proxy, service_proxy->GetObjectPath());
}

void PeerdClientImpl::OnPeerdServiceRemoved(
    const dbus::ObjectPath& object_path) {
  delegate_->OnPeerdServiceRemoved(object_path);
}

void PeerdClientImpl::OnPeerdServiceChanged(
    org::chromium::peerd::ServiceProxy* service_proxy,
    const std::string& property) {
  delegate_->OnPeerdServiceChanged(service_proxy,
                                   service_proxy->GetObjectPath(), property);
}

}  // namespace leaderd
