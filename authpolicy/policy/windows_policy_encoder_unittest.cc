// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "authpolicy/policy/policy_encoder_test_base.h"
#include "authpolicy/policy/windows_policy_encoder.h"
#include "authpolicy/policy/windows_policy_keys.h"
#include "bindings/authpolicy_containers.pb.h"

namespace policy {

using WindowsPolicy = authpolicy::protos::WindowsPolicy;

class WindowsPolicyEncoderTest : public PolicyEncoderTestBase<WindowsPolicy> {
 public:
  WindowsPolicyEncoderTest() = default;
  ~WindowsPolicyEncoderTest() override = default;

 protected:
  void EncodeDict(WindowsPolicy* policy, const RegistryDict* dict) override {
    WindowsPolicyEncoder encoder(dict);
    policy->Clear();
    encoder.EncodePolicy(policy);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowsPolicyEncoderTest);
};

// Test encoding of actual Windows policies.
TEST_F(WindowsPolicyEncoderTest, TestEncoding) {
  WindowsPolicy policy;

  EncodeInteger(&policy, kKeyUserPolicyMode, 0);
  EXPECT_EQ(WindowsPolicy::USER_POLICY_MODE_DEFAULT, policy.user_policy_mode());
  EncodeInteger(&policy, kKeyUserPolicyMode, 1);
  EXPECT_EQ(WindowsPolicy::USER_POLICY_MODE_MERGE, policy.user_policy_mode());
  EncodeInteger(&policy, kKeyUserPolicyMode, 2);
  EXPECT_EQ(WindowsPolicy::USER_POLICY_MODE_REPLACE, policy.user_policy_mode());
}

}  // namespace policy
