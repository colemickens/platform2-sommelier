// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_POLICY_ENCODER_TEST_BASE_H_
#define AUTHPOLICY_POLICY_POLICY_ENCODER_TEST_BASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <components/policy/core/common/registry_dict.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace policy {

// Base class for UserPolicyEncoderTest and DevicePolicyEncoderTest. T_POLICY
// is the type of the policy and should either be
// enterprise_management::CloudPolicySettings or
// enterprise_management::ChromeDeviceSettingsProto.
template <typename T_POLICY>
class PolicyEncoderTestBase : public ::testing::Test {
 public:
  PolicyEncoderTestBase() {}
  ~PolicyEncoderTestBase() override {}

 protected:
  // Clears |policy|, encodes |value| as value for the boolean policy |key| and
  // marks |key| as handled.
  void EncodeBoolean(T_POLICY* policy, const char* key, bool value) {
    EncodeValue(policy, key, base::MakeUnique<base::FundamentalValue>(value));
  }

  // Clears |policy|, encodes |value| as value for the integer policy |key| and
  // marks |key| as handled.
  void EncodeInteger(T_POLICY* policy, const char* key, int value) {
    EncodeValue(policy, key, base::MakeUnique<base::FundamentalValue>(value));
  }

  // Clears |policy|, encodes |value| as value for the string policy |key| and
  // marks |key| as handled.
  void EncodeString(T_POLICY* policy,
                    const char* key,
                    const std::string& value) {
    EncodeValue(policy, key, base::MakeUnique<base::StringValue>(value));
  }

  // Clears |policy|, encodes |value| as value for the string list policy |key|
  // and marks |key| as handled.
  void EncodeStringList(T_POLICY* policy,
                        const char* key,
                        const std::vector<std::string>& value) {
    auto value_dict = base::MakeUnique<RegistryDict>();
    for (int n = 0; n < static_cast<int>(value.size()); ++n) {
      value_dict->SetValue(base::IntToString(n + 1),
                           base::MakeUnique<base::StringValue>(value[n]));
    }
    RegistryDict root_dict;
    root_dict.SetKey(key, std::move(value_dict));
    EncodeDict(policy, &root_dict);
    MarkHandled(key);
  }

  // Called by all of the Encode* methods. Override to keep track of encoded
  // policies.
  virtual void MarkHandled(const char* /* key */) {}

  // Uses a policy encoder to write |dict| to |policy|.
  virtual void EncodeDict(T_POLICY* policy, const RegistryDict* dict) = 0;

 private:
  // Clears |policy|, encodes |value| as value for the given |key| and marks
  // |key| as handled.
  void EncodeValue(T_POLICY* policy,
                   const char* key,
                   std::unique_ptr<base::Value> value) {
    RegistryDict dict;
    dict.SetValue(key, std::move(value));
    EncodeDict(policy, &dict);
    MarkHandled(key);
  }

  DISALLOW_COPY_AND_ASSIGN(PolicyEncoderTestBase);
};

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_POLICY_ENCODER_TEST_BASE_H_
