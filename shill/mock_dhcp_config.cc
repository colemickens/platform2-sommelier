// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dhcp_config.h"

using std::string;

namespace shill {

MockDHCPConfig::MockDHCPConfig(ControlInterface *control_interface,
                               const string &device_name)
    : DHCPConfig(control_interface,
                 NULL,
                 NULL,
                 device_name,
                 string(),
                 string(),
                 false,
                 NULL) {}

MockDHCPConfig::~MockDHCPConfig() {}

}  // namespace shill
