// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_DEVICE_POLICY_H_
#define LOGIN_MANAGER_MOCK_DEVICE_POLICY_H_

#include "login_manager/device_policy.h"

#include <unistd.h>
#include <gmock/gmock.h>

namespace login_manager {
class MockDevicePolicy : public DevicePolicy {
 public:
  MockDevicePolicy() : DevicePolicy(FilePath("")) {}
  virtual ~MockDevicePolicy() {}
  MOCK_METHOD0(LoadOrCreate, bool(void));
  MOCK_METHOD0(Persist, bool(void));
  MOCK_CONST_METHOD1(Get, bool(std::string*));
  MOCK_METHOD1(Set, void(const enterprise_management::PolicyFetchResponse&));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_DEVICE_POLICY_H_
