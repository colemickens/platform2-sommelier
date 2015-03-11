// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brdebug/peerd_client.h"

#include <map>
#include <string>

#include <chromeos/errors/error.h>

namespace brdebug {

namespace {

const char kBrdebugServiceId[] = "brdebug";

void OnSuccess(const std::string& operation) {
  LOG(INFO) << operation << " succeeded.";
}

void OnError(const std::string& operation, chromeos::Error* error) {
  LOG(ERROR) << operation << " failed: " << error->GetMessage();
}

}  // namespace

PeerdClient::PeerdClient(const scoped_refptr<dbus::Bus>& bus)
    : peerd_object_manager_proxy_{bus} {
  peerd_object_manager_proxy_.SetManagerAddedCallback(
      base::Bind(&PeerdClient::OnPeerdOnline, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetManagerRemovedCallback(
      base::Bind(&PeerdClient::OnPeerdOffline, weak_ptr_factory_.GetWeakPtr()));
}

PeerdClient::~PeerdClient() {
  RemoveService();
}

void PeerdClient::OnPeerdOnline(
    org::chromium::peerd::ManagerProxy* manager_proxy) {
  peerd_manager_proxy_ = manager_proxy;
  VLOG(1) << "Peerd manager is online at '"
          << manager_proxy->GetObjectPath().value() << "'.";
  ExposeService();
}

void PeerdClient::OnPeerdOffline(const dbus::ObjectPath& object_path) {
  peerd_manager_proxy_ = nullptr;
  VLOG(1) << "Peerd manager is now offline.";
}

void PeerdClient::ExposeService() {
  if (peerd_manager_proxy_ == nullptr) return;

  // TODO(yimingc): Replace this with actual device properties (brbug.com/566).
  std::map<std::string, std::string> dev_props{
      {"test_key", "test_value"},
  };

  VLOG(1) << "Starting peerd advertising.";
  peerd_manager_proxy_->ExposeServiceAsync(
      kBrdebugServiceId, dev_props, {}, base::Bind(&OnSuccess, "ExposeService"),
      base::Bind(&OnError, "ExposeService"));
}

void PeerdClient::RemoveService() {
  if (peerd_manager_proxy_ == nullptr) return;

  VLOG(1) << "Stopping peerd advertising.";
  peerd_manager_proxy_->RemoveExposedServiceAsync(
      kBrdebugServiceId, base::Bind(&OnSuccess, "ExposeService"),
      base::Bind(&OnError, "RemoveService"));
}

}  // namespace brdebug
