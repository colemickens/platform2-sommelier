// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_power_manager.h"

namespace shill {

MockPowerManager::MockPowerManager(EventDispatcher* dispatcher,
                                   ControlInterface* control_interface)
    : PowerManager(dispatcher, control_interface) {}

MockPowerManager::~MockPowerManager() = default;

}  // namespace shill
