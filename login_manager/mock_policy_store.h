// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_POLICY_STORE_H_
#define LOGIN_MANAGER_MOCK_POLICY_STORE_H_

#include "login_manager/policy_store.h"

namespace login_manager {
class MockPolicyStore : public PolicyStore {
 public:
  MockPolicyStore();
  virtual ~MockPolicyStore();
  MOCK_METHOD0(DefunctPrefsFilePresent, bool(void));
  MOCK_METHOD0(EnsureLoadedOrCreated, bool(void));
  MOCK_CONST_METHOD0(Get,
                     const enterprise_management::PolicyFetchResponse&(void));
  MOCK_METHOD0(Persist, bool(void));
  MOCK_METHOD1(Set, void(const enterprise_management::PolicyFetchResponse&));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_POLICY_STORE_H_
