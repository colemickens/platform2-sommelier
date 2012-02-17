// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_POWER_MANAGER_
#define SHILL_MOCK_POWER_MANAGER_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/power_manager.h"

namespace shill {

class ProxyFactory;

class MockPowerManager : public PowerManager {
 public:
  explicit MockPowerManager(ProxyFactory *proxy_factory);
  virtual ~MockPowerManager();
  MOCK_METHOD2(AddStateChangeCallback,
               void(const std::string &key, PowerStateCallback *callback));
  MOCK_METHOD1(RemoveStateChangeCallback, void(const std::string &key));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPowerManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_POWER_MANAGER_
