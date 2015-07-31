// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/ap_manager_client.h"

namespace buffet {

using org::chromium::apmanager::ConfigProxy;
using org::chromium::apmanager::ManagerProxy;
using org::chromium::apmanager::ServiceProxy;

ApManagerClient::ApManagerClient(const scoped_refptr<dbus::Bus>& bus)
    : bus_(bus) {}

ApManagerClient::~ApManagerClient() {
  Stop();
}

void ApManagerClient::Start(const std::string& ssid) {
  if (service_path_.IsValid()) {
    return;
  }

  ssid_ = ssid;

  object_manager_proxy_.reset(
      new org::chromium::apmanager::ObjectManagerProxy{bus_});
  object_manager_proxy_->SetManagerAddedCallback(base::Bind(
      &ApManagerClient::OnManagerAdded, weak_ptr_factory_.GetWeakPtr()));
  object_manager_proxy_->SetServiceAddedCallback(base::Bind(
      &ApManagerClient::OnServiceAdded, weak_ptr_factory_.GetWeakPtr()));

  object_manager_proxy_->SetServiceRemovedCallback(base::Bind(
      &ApManagerClient::OnServiceRemoved, weak_ptr_factory_.GetWeakPtr()));
  object_manager_proxy_->SetManagerRemovedCallback(base::Bind(
      &ApManagerClient::OnManagerRemoved, weak_ptr_factory_.GetWeakPtr()));
}

void ApManagerClient::Stop() {
  if (manager_proxy_ && service_path_.IsValid()) {
    RemoveService(service_path_);
  }
  service_path_ = dbus::ObjectPath();
  service_proxy_ = nullptr;
  manager_proxy_ = nullptr;
  object_manager_proxy_.reset();
  ssid_.clear();
}

void ApManagerClient::RemoveService(const dbus::ObjectPath& object_path) {
  CHECK(object_path.IsValid());
  chromeos::ErrorPtr error;
  if (!manager_proxy_->RemoveService(object_path, &error)) {
    LOG(ERROR) << "RemoveService failed: " << error->GetMessage();
  }
}

void ApManagerClient::OnManagerAdded(ManagerProxy* manager_proxy) {
  VLOG(1) << "manager added: " << manager_proxy->GetObjectPath().value();
  manager_proxy_ = manager_proxy;

  if (service_path_.IsValid())
    return;

  chromeos::ErrorPtr error;
  if (!manager_proxy_->CreateService(&service_path_, &error)) {
    LOG(ERROR) << "CreateService failed: " << error->GetMessage();
  }
}

void ApManagerClient::OnServiceAdded(ServiceProxy* service_proxy) {
  VLOG(1) << "service added: " << service_proxy->GetObjectPath().value();
  if (service_proxy->GetObjectPath() != service_path_) {
    RemoveService(service_proxy->GetObjectPath());
    return;
  }
  service_proxy_ = service_proxy;

  ConfigProxy* config_proxy =
      object_manager_proxy_->GetConfigProxy(service_proxy->config());
  ConfigProxy::PropertySet* properties = config_proxy->GetProperties();
  properties->ssid.Set(ssid_, base::Bind(&ApManagerClient::OnSsidSet,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void ApManagerClient::OnSsidSet(bool success) {
  if (!success || !service_proxy_) {
    LOG(ERROR) << "Failed to set ssid.";
    return;
  }
  VLOG(1) << "SSID is set: " << ssid_;

  chromeos::ErrorPtr error;
  if (!service_proxy_->Start(&error)) {
    LOG(ERROR) << "Service start failed: " << error->GetMessage();
  }
}

void ApManagerClient::OnServiceRemoved(const dbus::ObjectPath& object_path) {
  VLOG(1) << "service removed: " << object_path.value();
  if (object_path != service_path_)
    return;
  service_path_ = dbus::ObjectPath();
  service_proxy_ = nullptr;
}

void ApManagerClient::OnManagerRemoved(const dbus::ObjectPath& object_path) {
  VLOG(1) << "manager removed: " << object_path.value();
  manager_proxy_ = nullptr;
  Stop();
}

}  // namespace buffet
