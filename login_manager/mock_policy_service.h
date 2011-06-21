// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_POLICY_SERVICE_H_
#define LOGIN_MANAGER_MOCK_POLICY_SERVICE_H_

#include "login_manager/policy_service.h"

namespace login_manager {
class MockPolicyService : public PolicyService {
 public:
  MockPolicyService();
  virtual ~MockPolicyService();
  MOCK_METHOD0(Initialize, bool(void));
  MOCK_METHOD4(Store, bool(const uint8*, uint32, Completion*, int));
  MOCK_METHOD1(Retrieve, bool(std::vector<uint8>*));
  MOCK_METHOD0(PersistKey, void(void));
  MOCK_METHOD1(PersistPolicy, void(Completion*));
  MOCK_METHOD0(PersistPolicySync, bool(void));
};

class MockPolicyServiceCompletion : public PolicyService::Completion {
 public:
  MockPolicyServiceCompletion();
  virtual ~MockPolicyServiceCompletion();
  MOCK_METHOD0(Success, void(void));
  MOCK_METHOD1(Failure, void(const PolicyService::Error&));
};

class MockPolicyServiceDelegate : public PolicyService::Delegate {
 public:
  MockPolicyServiceDelegate();
  virtual ~MockPolicyServiceDelegate();
  MOCK_METHOD1(OnPolicyPersisted, void(bool));
  MOCK_METHOD1(OnKeyPersisted, void(bool));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_POLICY_SERVICE_H_
