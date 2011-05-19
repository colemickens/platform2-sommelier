// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_USER_POLICY_SERVICE_FACTORY_H_
#define LOGIN_MANAGER_MOCK_USER_POLICY_SERVICE_FACTORY_H_

#include "login_manager/user_policy_service_factory.h"

namespace login_manager {

class MockUserPolicyServiceFactory : public UserPolicyServiceFactory {
 public:
  MockUserPolicyServiceFactory();
  virtual ~MockUserPolicyServiceFactory();
  MOCK_METHOD1(Create, PolicyService*(const std::string&));
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_USER_POLICY_SERVICE_FACTORY_H_
