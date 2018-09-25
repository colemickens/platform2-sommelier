// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_device_claimer.h"

namespace shill {

MockDeviceClaimer::MockDeviceClaimer(const std::string& dbus_service_name)
    : DeviceClaimer(dbus_service_name, nullptr, false) {}

MockDeviceClaimer::~MockDeviceClaimer() {}

}  // namespace shill
