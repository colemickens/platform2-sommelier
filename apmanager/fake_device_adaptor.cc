// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/fake_device_adaptor.h"

namespace apmanager {

FakeDeviceAdaptor::FakeDeviceAdaptor() : in_use_(false) {}

FakeDeviceAdaptor::~FakeDeviceAdaptor() {}

void FakeDeviceAdaptor::SetDeviceName(const std::string& device_name) {
  device_name_ = device_name;
}

std::string FakeDeviceAdaptor::GetDeviceName() {
  return device_name_;
}

void FakeDeviceAdaptor::SetPreferredApInterface(
    const std::string& interface_name) {
  preferred_ap_interface_ = interface_name;
}

std::string FakeDeviceAdaptor::GetPreferredApInterface() {
  return preferred_ap_interface_;
}

void FakeDeviceAdaptor::SetInUse(bool in_use) {
  in_use_ = in_use;
}

bool FakeDeviceAdaptor::GetInUse() {
  return in_use_;
}

}  // namespace apmanager
