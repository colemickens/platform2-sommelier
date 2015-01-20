// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/peerd_client.h"

#include <map>

#include <base/strings/string_util.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/wifi_bootstrap_manager.h"
#include "privetd/wifi_ssid_generator.h"

namespace privetd {

PeerdClient::PeerdClient(const scoped_refptr<dbus::Bus>& bus,
                         const DeviceDelegate* device,
                         const CloudDelegate* cloud,
                         const WifiDelegate* wifi)
    : peerd_object_manager_proxy_{bus},
      device_{device},
      cloud_{cloud},
      wifi_{wifi} {
  CHECK(device_);
  peerd_object_manager_proxy_.SetManagerAddedCallback(
      base::Bind(&PeerdClient::OnPeerdOnline, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetManagerRemovedCallback(
      base::Bind(&PeerdClient::OnPeerdOffline, weak_ptr_factory_.GetWeakPtr()));
}

PeerdClient::~PeerdClient() {
  Stop();
}

void PeerdClient::OnPeerdOnline(
    org::chromium::peerd::ManagerProxy* manager_proxy) {
  peerd_manager_proxy_ = manager_proxy;
  VLOG(1) << "Peerd manager is online at '"
          << manager_proxy->GetObjectPath().value() << "'.";
  Start();
}

void PeerdClient::OnPeerdOffline(const dbus::ObjectPath& object_path) {
  peerd_manager_proxy_ = nullptr;
  service_token_.clear();
  VLOG(1) << "Peerd manager is now offline.";
}

void PeerdClient::Start() {
  CHECK(service_token_.empty());

  // If peerd hasn't started yet, don't do anything.
  if (peerd_manager_proxy_ == nullptr)
    return;

  VLOG(1) << "Starting peerd advertising.";
  const uint16_t port = device_->GetHttpEnpoint().first;
  std::map<std::string, chromeos::Any> mdns_options{
      {"port", chromeos::Any{port}},
  };

  DCHECK_NE(port, 0);
  DCHECK(!device_->GetName().empty());
  DCHECK(!device_->GetId().empty());
  DCHECK_EQ(device_->GetClass().size(), 2U);
  DCHECK_EQ(device_->GetModelId().size(), 3U);

  std::string services;
  if (!device_->GetServices().empty())
    services += "_";
  services += JoinString(device_->GetServices(), ",_");

  std::map<std::string, std::string> txt_record{
      {"txtvers", "3"},
      {"ty", device_->GetName()},
      {"services", services},
      {"id", device_->GetId()},
      {"class", device_->GetClass()},
      {"model_id", device_->GetModelId()},
      {"flags", WifiSsidGenerator{device_, cloud_, wifi_}.GenerateFlags()},
  };

  if (cloud_ && !cloud_->GetCloudId().empty())
    txt_record.emplace("gcd_id", cloud_->GetCloudId());

  if (!device_->GetDescription().empty())
    txt_record.emplace("note", device_->GetDescription());

  chromeos::ErrorPtr error;
  if (!peerd_manager_proxy_->ExposeService("privet", txt_record,
                                           {{"mdns", mdns_options}},
                                           &service_token_, &error)) {
    LOG(ERROR) << "ExposeService failed:" << error->GetMessage();
  }
}

void PeerdClient::Stop() {
  if (service_token_.empty() || peerd_manager_proxy_ == nullptr)
    return;

  VLOG(1) << "Stopping peerd advertising.";
  chromeos::ErrorPtr error;
  if (!peerd_manager_proxy_->RemoveExposedService(service_token_, &error)) {
    LOG(ERROR) << "RemoveExposedService failed:" << error->GetMessage();
  }
  service_token_.clear();
}

}  // namespace privetd
