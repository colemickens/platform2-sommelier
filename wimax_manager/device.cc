// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/device.h"

#include <base/logging.h>

#include "wimax_manager/device_dbus_adaptor.h"

using std::string;

namespace wimax_manager {

namespace {

const size_t kMACAddressLength = 6;
const size_t kBaseStationIdLength = 6;

// Default time interval, in seconds, between network scans.
// TODO(benchan): Perform some measurements and fine tune this value.
const int kDefaultScanIntervalInSeconds = 5;

string GetDeviceStatusDescription(DeviceStatus device_status) {
  switch (device_status) {
    case kDeviceStatusUninitialized:
      return "Uninitialized";
    case kDeviceStatusDisabled:
      return "Disabled";
    case kDeviceStatusReady:
      return "Ready";
    case kDeviceStatusScanning:
      return "Scanning";
    case kDeviceStatusConnecting:
      return "Connecting";
    case kDeviceStatusConnected:
      return "Connected";
  }
  return "Unknown";
}

}  // namespace

Device::Device(uint8 index, const string &name)
    : index_(index),
      name_(name),
      mac_address_(kMACAddressLength),
      base_station_id_(kBaseStationIdLength),
      frequency_(0),
      scan_interval_(kDefaultScanIntervalInSeconds),
      status_(kDeviceStatusUninitialized),
      entering_suspend_mode_(false) {
}

Device::~Device() {
}

void Device::UpdateNetworks() {
  dbus_adaptor()->UpdateNetworks();
}

void Device::UpdateRFInfo() {
  dbus_adaptor()->UpdateRFInfo();
}

void Device::SetMACAddress(const ByteIdentifier &mac_address) {
  mac_address_.CopyFrom(mac_address);
  dbus_adaptor()->UpdateMACAddress();
}

void Device::SetBaseStationId(const ByteIdentifier &base_station_id) {
  base_station_id_.CopyFrom(base_station_id);
}

void Device::SetStatus(DeviceStatus status) {
  if (status_ != status) {
    LOG(INFO) << "Device status changed from "
              << GetDeviceStatusDescription(status_) << " to "
              << GetDeviceStatusDescription(status);
    status_ = status;
    dbus_adaptor()->UpdateStatus();
  }
}

}  // namespace wimax_manager
