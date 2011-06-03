// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"

#include <base/logging.h>

#include "shill/device.h"

using std::string;

namespace shill {

IPConfig::IPConfig(const Device &device) : device_(device) {
  VLOG(2) << "IPConfig created.";
}

IPConfig::~IPConfig() {
  VLOG(2) << "IPConfig destroyed.";
}

const string &IPConfig::GetDeviceName() const {
  return device().UniqueName();
}

bool IPConfig::Request() {
  return false;
}

bool IPConfig::Renew() {
  return false;
}

void IPConfig::UpdateProperties(const Properties &properties) {
  properties_ = properties;
  // TODO(petkov): Notify listeners about the new properties.
}

}  // namespace shill
