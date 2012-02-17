// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_power_manager.h"

namespace shill {

MockPowerManager::MockPowerManager(ProxyFactory *proxy_factory)
      : PowerManager(proxy_factory) {}

MockPowerManager::~MockPowerManager() {}

}  // namespace shill
