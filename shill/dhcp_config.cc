// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_config.h"

#include <base/logging.h>

namespace shill {

DHCPConfig::DHCPConfig(const Device &device) : IPConfig(device) {
  VLOG(2) << "DHCPConfig created.";
}

DHCPConfig::~DHCPConfig() {
  VLOG(2) << "DHCPConfig destroyed.";
}

}  // namespace shill
