// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_device_info.h"

namespace shill {

MockDeviceInfo::MockDeviceInfo(ControlInterface* control_interface,
                               EventDispatcher* dispatcher,
                               Metrics* metrics,
                               Manager* manager)
    : DeviceInfo(control_interface, dispatcher, metrics, manager) {}

MockDeviceInfo::~MockDeviceInfo() = default;

}  // namespace shill
