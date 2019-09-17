// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_POLICY_KEY_H_
#define LOGIN_MANAGER_MOCK_POLICY_KEY_H_

#include "login_manager/policy_key.h"

#include <stdint.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <gmock/gmock.h>

namespace base {
class RSAPrivateKey;
}

namespace login_manager {
class MockPolicyKey : public PolicyKey {
 public:
  MockPolicyKey();
  ~MockPolicyKey() override;
  MOCK_CONST_METHOD1(Equals, bool(const std::string&));
  MOCK_CONST_METHOD1(VEquals, bool(const std::vector<uint8_t>&));
  MOCK_CONST_METHOD0(HaveCheckedDisk, bool());
  MOCK_CONST_METHOD0(IsPopulated, bool());
  MOCK_METHOD0(PopulateFromDiskIfPossible, bool());
  MOCK_METHOD1(PopulateFromBuffer, bool(const std::vector<uint8_t>&));
  MOCK_METHOD1(PopulateFromKeypair, bool(crypto::RSAPrivateKey*));
  MOCK_METHOD0(Persist, bool());
  MOCK_METHOD2(Rotate,
               bool(const std::vector<uint8_t>&, const std::vector<uint8_t>&));
  MOCK_METHOD1(ClobberCompromisedKey, bool(const std::vector<uint8_t>&));
  MOCK_METHOD2(Verify,
               bool(const std::vector<uint8_t>&, const std::vector<uint8_t>&));
  MOCK_CONST_METHOD0(public_key_der, const std::vector<uint8_t>&());
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_POLICY_KEY_H_
