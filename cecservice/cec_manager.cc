// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "cecservice/cec_manager.h"

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>

namespace cecservice {

CecManager::CecManager(const UdevFactory& udev_factory,
                       const CecDeviceFactory& cec_factory)
    : cec_factory_(cec_factory) {
  udev_ = udev_factory.Create(
      base::Bind(&CecManager::OnDeviceAdded, weak_factory_.GetWeakPtr()),
      base::Bind(&CecManager::OnDeviceRemoved, weak_factory_.GetWeakPtr()));
  LOG_IF(FATAL, !udev_) << "Failed to create udev.";

  EnumerateAndAddExistingDevices();
}

CecManager::~CecManager() = default;

void CecManager::SetWakeUp() {
  LOG(INFO) << "Received wake up request.";
  for (auto& kv : devices_) {
    kv.second->SetWakeUp();
  }
}

void CecManager::SetStandBy() {
  LOG(INFO) << "Received standby request.";
  for (auto& kv : devices_) {
    kv.second->SetStandBy();
  }
}

void CecManager::OnDeviceAdded(const base::FilePath& device_path) {
  LOG(INFO) << "New device: " << device_path.value();
  AddNewDevice(device_path);
}

void CecManager::OnDeviceRemoved(const base::FilePath& device_path) {
  LOG(INFO) << "Removing device: " << device_path.value();
  devices_.erase(device_path);
}

void CecManager::EnumerateAndAddExistingDevices() {
  std::vector<base::FilePath> paths;
  if (!udev_->EnumerateDevices(&paths)) {
    LOG(FATAL) << "Failed to enumerate devices.";
  }
  for (const auto& path : paths) {
    AddNewDevice(path);
  }
}

void CecManager::AddNewDevice(const base::FilePath& path) {
  std::unique_ptr<CecDevice> device = cec_factory_.Create(path);
  if (device) {
    LOG(INFO) << "Added new device: " << path.value();
    devices_[path] = std::move(device);
  } else {
    LOG(WARNING) << "Failed to add device: " << path.value();
  }
}

}  // namespace cecservice
