// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_USER_POLICY_ENCODER_H_
#define AUTHPOLICY_POLICY_USER_POLICY_ENCODER_H_

#include <vector>

#include <base/bind.h>
#include <components/policy/core/common/policy_types.h>

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

struct BooleanPolicyAccess;
struct IntegerPolicyAccess;
struct StringPolicyAccess;
struct StringListPolicyAccess;

// Private helper class used to convert a RegistryDict into a user policy
// protobuf. Don't include directly, use |preg_policy_encoder.h| instead.
class UserPolicyEncoder {
 public:
  UserPolicyEncoder(const RegistryDict* dict, PolicyLevel level);

  // Extracts all user policies from |dict_| and puts them into |policy|.
  void EncodePolicy(enterprise_management::CloudPolicySettings* policy) const;

 private:
  // Marks a policy recommended or mandatory.
  void SetPolicyOptions(enterprise_management::PolicyOptions* options) const;

  // Gets a PolicyLevel as string.
  const char* GetLevelStr() const;

  // Boolean policies.
  void EncodeBoolean(enterprise_management::CloudPolicySettings* policy,
                     const BooleanPolicyAccess* access) const;

  // Integer policies.
  void EncodeInteger(enterprise_management::CloudPolicySettings* policy,
                     const IntegerPolicyAccess* access) const;

  // String policies.
  void EncodeString(enterprise_management::CloudPolicySettings* policy,
                    const StringPolicyAccess* access) const;

  // String list policies are a little different. Unlike the basic types they
  // are not stored as registry value, but as registry key with values 1, 2, ...
  // for the entries.
  void EncodeStringList(enterprise_management::CloudPolicySettings* policy,
                        const StringListPolicyAccess* access) const;

 private:
  // Template for the Encode*() methods above, to be passed into EncodeList().
  template <typename T_Access>
  using Encoder = void (UserPolicyEncoder::*)(
      enterprise_management::CloudPolicySettings* policy,
      const T_Access* access) const;

  // Encodes all policies of one of the types above. |access| is a pointer to a
  // NULL-terminated list of policies of a certain type from policy_constants.h.
  // |encode| is a pointer to one of the encoders above, e.g.
  // &UserPolicyEncoder::EncodeBoolean.
  template <typename T_Access>
  void EncodeList(enterprise_management::CloudPolicySettings* policy,
                  const T_Access* access,
                  Encoder<T_Access> encode) const;

  const RegistryDict* dict_;
  PolicyLevel level_;
};

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_USER_POLICY_ENCODER_H_
