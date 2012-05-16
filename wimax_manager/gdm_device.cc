// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/gdm_device.h"

#include <base/logging.h>
#include <base/memory/scoped_vector.h>
#include <base/stl_util.h>

#include "wimax_manager/gdm_driver.h"
#include "wimax_manager/network.h"

using std::string;
using std::vector;

namespace wimax_manager {

namespace {

const int kMaxNumberOfTrials = 10;

}  // namespace

GdmDevice::GdmDevice(uint8 index, const string &name,
                     const base::WeakPtr<GdmDriver> &driver)
    : Device(index, name),
      driver_(driver),
      open_(false),
      status_(WIMAX_API_DEVICE_STATUS_UnInitialized),
      connection_progress_(WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging) {
}

GdmDevice::~GdmDevice() {
  Disable();
  Close();
}

bool GdmDevice::Open() {
  if (!driver_)
    return false;

  if (open_)
    return true;

  if (!driver_->OpenDevice(this)) {
    LOG(ERROR) << "Failed to open device '" << name() << "'";
    return false;
  }

  open_ = true;
  return true;
}

bool GdmDevice::Close() {
  if (!driver_)
    return false;

  if (!open_)
    return true;

  if (!driver_->CloseDevice(this)) {
    LOG(ERROR) << "Failed to close device '" << name() << "'";
    return false;
  }

  open_ = false;
  return true;
}

bool GdmDevice::Enable() {
  if (!Open())
    return false;

  if (!driver_->SetDeviceEAPParameters(this)) {
    LOG(ERROR) << "Failed to set EAP parameters of device '" << name() << "'";
    return false;
  }

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }

  if (!driver_->AutoSelectProfileForDevice(this)) {
    LOG(ERROR) << "Failed to auto select profile for device '" << name() << "'";
    return false;
  }

  if (!driver_->PowerOnDeviceRF(this)) {
    LOG(ERROR) << "Failed to power on RF of device '" << name() << "'";
    return false;
  }

  return true;
}

bool GdmDevice::Disable() {
  if (!Open())
    return false;

  if (!driver_->PowerOffDeviceRF(this)) {
    LOG(ERROR) << "Failed to power off RF of device '" << name() << "'";
    return false;
  }

  return true;
}

bool GdmDevice::Connect() {
  if (!Open())
    return false;

  ScopedVector<Network> networks;
  for (int num_trials = 0; num_trials < kMaxNumberOfTrials; ++num_trials) {
    // TODO(benchan): Fix this code.
    sleep(2);
    if (!driver_->GetNetworksForDevice(this, &networks.get())) {
      LOG(ERROR) << "Failed to get list of networks for device '"
                 << name() << "'";
    }
    if (!networks.empty())
      break;
  }

  if (networks.empty())
    return false;

  bool success = driver_->ConnectDeviceToNetwork(this, networks[0]);
  if (!success)
    LOG(ERROR) << "Failed to connect device '" << name() << "' to network";

  return success;
}

bool GdmDevice::Disconnect() {
  if (!Open())
    return false;

  if (!driver_->DisconnectDeviceFromNetwork(this)) {
    LOG(ERROR) << "Failed to disconnect device '" << name() << "' from network";
    return false;
  }

  return true;
}

void GdmDevice::set_mac_address(const uint8 mac_address[6]) {
  memcpy(mac_address_, mac_address, sizeof(mac_address_));
}

}  // namespace wimax_manager
