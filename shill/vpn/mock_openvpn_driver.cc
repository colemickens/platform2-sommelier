// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn/mock_openvpn_driver.h"

namespace shill {

MockOpenVPNDriver::MockOpenVPNDriver()
    : OpenVPNDriver(nullptr, nullptr, nullptr) {}

MockOpenVPNDriver::~MockOpenVPNDriver() = default;

}  // namespace shill
