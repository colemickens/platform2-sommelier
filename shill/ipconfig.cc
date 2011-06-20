// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"

#include <base/logging.h>

#include "shill/device.h"

using std::string;

namespace shill {

IPConfig::IPConfig(DeviceConstRefPtr device) : device_(device) {
  VLOG(2) << "IPConfig created.";
}

IPConfig::~IPConfig() {
  VLOG(2) << "IPConfig destroyed.";
}

const string &IPConfig::GetDeviceName() const {
  return device()->UniqueName();
}

bool IPConfig::RequestIP() {
  return false;
}

bool IPConfig::RenewIP() {
  return false;
}

bool IPConfig::ReleaseIP() {
  return false;
}

void IPConfig::UpdateProperties(const Properties &properties, bool success) {
  properties_ = properties;
  if (update_callback_.get()) {
    update_callback_->Run(this, success);
  }
}

void IPConfig::RegisterUpdateCallback(
    Callback2<IPConfigRefPtr, bool>::Type *callback) {
  update_callback_.reset(callback);
}

}  // namespace shill
