// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dhcp_config.h"

namespace shill {

MockDHCPConfig::MockDHCPConfig(ControlInterface *control_interface,
                               EventDispatcher *dispatcher,
                               DHCPProvider *provider,
                               const std::string &device_name,
                               GLib *glib)
    : DHCPConfig(control_interface,
                 dispatcher,
                 provider,
                 device_name,
                 glib) {}

MockDHCPConfig::~MockDHCPConfig() {}

}  // namespace shill
