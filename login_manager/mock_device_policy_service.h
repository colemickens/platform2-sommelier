// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_DEVICE_POLICY_SERVICE_H_
#define LOGIN_MANAGER_MOCK_DEVICE_POLICY_SERVICE_H_

#include "login_manager/device_policy_service.h"

namespace login_manager {
class MockDevicePolicyService : public DevicePolicyService {
 public:
  MockDevicePolicyService();
  virtual ~MockDevicePolicyService();
  MOCK_METHOD0(Initialize, bool(void));
  MOCK_METHOD4(Store, bool(const uint8*, uint32, Completion*, int));
  MOCK_METHOD1(Retrieve, bool(std::vector<uint8>*));
  MOCK_METHOD0(PersistKey, void(void));
  MOCK_METHOD1(PersistPolicy, void(Completion*));
  MOCK_METHOD0(PersistPolicySync, bool(void));
  MOCK_METHOD3(CheckAndHandleOwnerLogin, bool(const std::string&,
                                              bool*,
                                              PolicyService::Error*));
  MOCK_METHOD2(ValidateAndStoreOwnerKey, bool(const std::string&,
                                              const std::string&));
  MOCK_METHOD0(KeyMissing, bool(void));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_DEVICE_POLICY_SERVICE_H_
