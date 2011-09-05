// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libpolicy.h"

#include <base/logging.h>

#include "device_policy_impl.h"

namespace policy {

PolicyProvider::PolicyProvider()
    : device_policy_(new DevicePolicyImpl),
      device_policy_is_loaded_(false) {
  // Force initial load of the policy contents.
  Reload();
}

PolicyProvider::PolicyProvider(DevicePolicy* device_policy)
    : device_policy_(device_policy),
      device_policy_is_loaded_(true) {
}

PolicyProvider::~PolicyProvider() {
}

bool PolicyProvider::Reload() {
  device_policy_is_loaded_ = device_policy_->LoadPolicy();
  if (!device_policy_is_loaded_) {
    LOG(WARNING) << "Could not load the device policy file.";
  }
  return device_policy_is_loaded_;
}

bool PolicyProvider::device_policy_is_loaded() const {
  return device_policy_is_loaded_;
}

const DevicePolicy& PolicyProvider::GetDevicePolicy() const {
  if (!device_policy_is_loaded_)
    DCHECK("Trying to get policy data but policy was not loaded!");

  return *device_policy_;
}

}  // namespace policy
