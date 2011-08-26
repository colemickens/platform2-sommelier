// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_ipconfig.h"

namespace shill {

MockIPConfig::MockIPConfig(ControlInterface *control_interface,
                           const std::string &device_name)
    : IPConfig(control_interface, device_name) {}

MockIPConfig::~MockIPConfig() {}

}  // namespace shill
