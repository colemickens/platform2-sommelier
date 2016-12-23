// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_USER_POLICY_ENCODER_H_
#define AUTHPOLICY_POLICY_USER_POLICY_ENCODER_H_

#include <vector>

#include "base/bind.h"

namespace enterprise_management {
class PolicyOptions;
class BooleanPolicyProto;
class IntegerPolicyProto;
class StringPolicyProto;
class StringListPolicyProto;
class CloudPolicySettings;
}  // namespace enterprise_management

namespace policy {

class RegistryDict;

enum PolicyLevel {
  POLICY_LEVEL_RECOMMENDED,  // Values are defaults, can be overridden by users.
  POLICY_LEVEL_MANDATORY,    // Values are enforced on user.
};

// Private helper class used to convert a RegistryDict into a user policy
// protobuf. Don't include directly, use |preg_policy_encoder.h| instead.
class UserPolicyEncoder {
 public:
  UserPolicyEncoder(const RegistryDict* dict, PolicyLevel level)
      : dict_(dict), level_(level) {}

  // Extracts all user policies from |dict_| and puts them into |policy|.
  void EncodeUserPolicy(
      enterprise_management::CloudPolicySettings* policy) const;

 private:
  // Note: Split up into two functions to satisfy the linter. This will go away
  // once the code is auto-generated at compile time.
  void EncodeUserPolicy1(
      enterprise_management::CloudPolicySettings* policy) const;
  void EncodeUserPolicy2(
      enterprise_management::CloudPolicySettings* policy) const;

  // We only want to create protobuf fields for policies that are actually
  // contained in the registry dictionary. Therefore, it is necessary to use
  // callbacks to mutable_<policy name> methods instead of calling them
  // directly.
  using BooleanPolicyCallback =
      base::Callback<enterprise_management::BooleanPolicyProto*()>;
  using IntegerPolicyCallback =
      base::Callback<enterprise_management::IntegerPolicyProto*()>;
  using StringPolicyCallback =
      base::Callback<enterprise_management::StringPolicyProto*()>;
  using StringListPolicyCallback =
      base::Callback<enterprise_management::StringListPolicyProto*()>;

  // Marks a policy recommended or mandatory.
  void SetPolicyOptions(enterprise_management::PolicyOptions* options) const;

  // Gets a PolicyLevel as string.
  const char* GetLevelStr() const;

  // Boolean policies.
  void EncodeBoolean(const char* policy_name,
                     const BooleanPolicyCallback& create_proto) const;

  // Integer policies.
  void EncodeInteger(const char* policy_name,
                     const IntegerPolicyCallback& create_proto) const;

  // String policies.
  void EncodeString(const char* policy_name,
                    const StringPolicyCallback& create_proto) const;

  // String list policies are a little different. Unlike the basic types they
  // are not stored as registry value, but as registry key with values 1, 2, ...
  // for the entries.
  void EncodeStringList(const char* policy_name,
                        const StringListPolicyCallback& create_proto) const;

 private:
  const RegistryDict* dict_;
  PolicyLevel level_;
};

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_USER_POLICY_ENCODER_H_
