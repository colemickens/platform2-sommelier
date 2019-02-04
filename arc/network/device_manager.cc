// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device_manager.h"

#include <utility>

namespace arc_networkd {

DeviceManager::DeviceManager(const Device::MessageSink& msg_sink)
    : msg_sink_(msg_sink) {
  Add(kAndroidDevice);
}

DeviceManager::~DeviceManager() {}

size_t DeviceManager::Reset(const std::set<std::string>& devices) {
  for (auto it = devices_.begin(); it != devices_.end();) {
    const std::string& name = it->first;
    if (name != kAndroidDevice && devices.find(name) == devices.end()) {
      LOG(INFO) << "Removing device " << name;
      it = devices_.erase(it);
    } else {
      ++it;
    }
  }
  for (const std::string& name : devices) {
    Add(name);
  }
  return devices_.size();
}

bool DeviceManager::Add(const std::string& name) {
  if (devices_.find(name) != devices_.end())
    return false;

  auto device = Device::ForInterface(name, msg_sink_);
  if (!device)
    return false;

  LOG(INFO) << "Adding device " << name;
  devices_.emplace(name, std::move(device));
  return true;
}

bool DeviceManager::Enable(const std::string& name, const std::string& ifname) {
  // This is only applicable to the legacy 'android' interface.
  if (name != kAndroidDevice)
    return false;

  const auto it = devices_.find(kAndroidDevice);
  if (it != devices_.end()) {
    it->second->Disable();
    if (!ifname.empty())
      it->second->Enable(ifname);
  }
  return true;
}

bool DeviceManager::Disable(const std::string& name) {
  return Enable(name, "");
}

void DeviceManager::DisableAll() {
  for (const auto& d : devices_) {
    Disable(d.first);
  }
}

}  // namespace arc_networkd
