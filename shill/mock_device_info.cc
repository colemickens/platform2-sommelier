// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_device_info.h"

namespace shill {

MockDeviceInfo::MockDeviceInfo(ControlInterface *control_interface,
                               EventDispatcher *dispatcher,
                               Manager *manager)
    : DeviceInfo(control_interface, dispatcher, manager) {}

MockDeviceInfo::~MockDeviceInfo() {}

}  // namespace shill
