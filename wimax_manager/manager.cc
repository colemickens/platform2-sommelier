// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/manager.h"

#include <base/logging.h>
#include <base/stl_util.h>

#include "wimax_manager/device.h"
#include "wimax_manager/device_dbus_adaptor.h"
#include "wimax_manager/gdm_driver.h"
#include "wimax_manager/manager_dbus_adaptor.h"

namespace wimax_manager {

namespace {

const int kMaxNumberOfDeviceScans = 10;
const int kDefaultDeviceScanIntervalInSeconds = 3;

gboolean OnDeviceScanNeeded(gpointer data) {
  CHECK(data);

  reinterpret_cast<Manager *>(data)->ScanDevices();
  // ScanDevices decides if a rescan is needed later, so return FALSE
  // to prevent this function from being called repeatedly.
  return FALSE;
}

}  // namespace

Manager::Manager() : num_device_scans_(0), device_scan_timeout_id_(0) {
}

Manager::~Manager() {
  Finalize();
}

bool Manager::Initialize() {
  if (driver_.get())
    return true;

  driver_.reset(new(std::nothrow) GdmDriver());
  if (!driver_.get()) {
    LOG(ERROR) << "Failed to create driver";
    return false;
  }

  if (!driver_->Initialize()) {
    LOG(ERROR) << "Failed to initialize driver";
    return false;
  }

  if (!ScanDevices())
    return false;

  return true;
}

bool Manager::Finalize() {
  // Cancel any pending device scan.
  if (device_scan_timeout_id_ != 0) {
    g_source_remove(device_scan_timeout_id_);
    device_scan_timeout_id_ = 0;
  }
  num_device_scans_ = 0;

  devices_.reset();

  if (!driver_.get())
    return true;

  if (!driver_->Finalize()) {
    LOG(ERROR) << "Failed to de-initialize driver";
    return false;
  }

  driver_.reset();
  return true;
}

bool Manager::ScanDevices() {
  device_scan_timeout_id_ = 0;

  if (!devices_.empty())
    return true;

  if (!driver_->GetDevices(&devices_.get())) {
    LOG(ERROR) << "Failed to get list of devices";
    return false;
  }

  if (!devices_.empty()) {
    for (size_t i = 0; i < devices_.size(); ++i)
      devices_[i]->CreateDBusAdaptor();

    dbus_adaptor()->UpdateDevices();
    return true;
  }

  LOG(INFO) << "No WiMAX devices detected. Rescan later.";
  // Some platforms may not have any WiMAX device, so instead of scanning
  // indefinitely, stop the device scan after a number of attempts.
  if (++num_device_scans_ < kMaxNumberOfDeviceScans) {
    device_scan_timeout_id_ = g_timeout_add_seconds(
        kDefaultDeviceScanIntervalInSeconds, OnDeviceScanNeeded, this);
  }
  return true;
}

}  // namespace wimax_manager
