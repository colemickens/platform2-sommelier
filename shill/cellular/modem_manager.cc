// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem_manager.h"

#include <memory>
#include <utility>

#include <base/stl_util.h>

#include "shill/cellular/modem.h"
#include "shill/control_interface.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/manager.h"

using std::string;

namespace shill {

ModemManager::ModemManager(const string& service,
                           const string& path,
                           ModemInfo* modem_info)
    : service_(service),
      path_(path),
      service_connected_(false),
      modem_info_(modem_info) {}

ModemManager::~ModemManager() = default;

void ModemManager::Connect() {
  // Inheriting classes call this superclass method.
  service_connected_ = true;
}

void ModemManager::Disconnect() {
  // Inheriting classes call this superclass method.
  modems_.clear();
  service_connected_ = false;
}

void ModemManager::OnAppeared() {
  LOG(INFO) << "Modem manager " << service_ << " appeared.";
  Connect();
}

void ModemManager::OnVanished() {
  LOG(INFO) << "Modem manager " << service_ << " vanished.";
  Disconnect();
}

bool ModemManager::ModemExists(const std::string& path) const {
  CHECK(service_connected_);
  if (base::ContainsKey(modems_, path)) {
    LOG(INFO) << "ModemExists: " << path << " already exists.";
    return true;
  } else {
    return false;
  }
}

void ModemManager::RecordAddedModem(std::unique_ptr<Modem> modem) {
  modems_[modem->path()] = std::move(modem);
}

void ModemManager::RemoveModem(const string& path) {
  LOG(INFO) << "Remove modem: " << path;
  CHECK(service_connected_);
  modems_.erase(path);
}

void ModemManager::OnDeviceInfoAvailable(const string& link_name) {
  for (const auto& link_name_modem_pair : modems_) {
    link_name_modem_pair.second->OnDeviceInfoAvailable(link_name);
  }
}

}  // namespace shill
