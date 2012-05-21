// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/device.h"

#include <base/logging.h>

#include "wimax_manager/device_dbus_adaptor.h"

using std::string;

namespace wimax_manager {

namespace {

// Default time interval, in seconds, between network scans.
// TODO(benchan): Perform some measurements and fine tune this value.
const int kDefaultScanIntervalInSeconds = 5;

}  // namespace

Device::Device(uint8 index, const string &name)
    : index_(index), name_(name),
      scan_interval_(kDefaultScanIntervalInSeconds) {
}

Device::~Device() {
}

void Device::UpdateNetworks() {
  dbus_adaptor()->UpdateNetworks();
}

}  // namespace wimax_manager
