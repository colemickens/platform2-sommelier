// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp/mock_dhcp_config.h"

using std::string;

namespace shill {

MockDHCPConfig::MockDHCPConfig(ControlInterface* control_interface,
                               const string& device_name)
    : DHCPConfig(control_interface,
                 nullptr,
                 nullptr,
                 device_name,
                 string(),
                 string()) {}

MockDHCPConfig::~MockDHCPConfig() = default;

void MockDHCPConfig::ProcessEventSignal(const std::string& reason,
                                        const KeyValueStore& configuration) {}
void MockDHCPConfig::ProcessStatusChangeSignal(const std::string& status) {}

}  // namespace shill
