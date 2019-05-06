// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_ppp_device.h"

namespace shill {

MockPPPDevice::MockPPPDevice(ControlInterface* control,
                             EventDispatcher* dispatcher,
                             Metrics* metrics,
                             Manager* manager,
                             const std::string& link_name,
                             int interface_index)
    : PPPDevice(control, dispatcher, metrics, manager, link_name,
                interface_index) {}

MockPPPDevice::~MockPPPDevice() = default;

}  // namespace shill
