// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
  MOCK_CONST_METHOD0(Get, const enterprise_management::PolicyFetchResponse&());
  MOCK_CONST_METHOD1(SerializeToString, bool(std::string*));
  MOCK_METHOD1(Set, void(const enterprise_management::PolicyFetchResponse&));
  MOCK_METHOD3(StoreOwnerProperties, bool(OwnerKey*,
                                          const std::string&,
                                          GError**));
  MOCK_METHOD1(CurrentUserIsOwner, bool(const std::string&));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_DEVICE_POLICY_H_
