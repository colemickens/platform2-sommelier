// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_DEVICE_POLICY_ENCODER_H_
#define AUTHPOLICY_POLICY_DEVICE_POLICY_ENCODER_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}  // namespace enterprise_management

namespace policy {

// Connection types for the key::kDeviceUpdateAllowedConnectionTypes policy,
// exposed for tests. The int is actually enterprise_management::
// AutoUpdateSettingsProto_ConnectionType, but we don't want to include
// chrome_device_policy.pb.h here in this header.
extern const std::pair<const char*, int> kConnectionTypes[];
extern const size_t kConnectionTypesSize;

class RegistryDict;

// Private helper class used to convert a RegistryDict into a device policy
// protobuf. Don't include directly, use |preg_policy_encoder.h| instead,
class DevicePolicyEncoder {
 public:
  explicit DevicePolicyEncoder(const RegistryDict* dict) : dict_(dict) {}

  // Toggles logging of policy values.
  void LogPolicyValues(bool enabled) { log_policy_values_ = enabled; }

  // Extracts all supported device policies from |dict| and puts them into
  // |policy|.
  void EncodePolicy(
      enterprise_management::ChromeDeviceSettingsProto* policy) const;

 private:
  // Callbacks to set policy values. StringListPolicyCallback actually appends
  // a string to the list. It does not set the whole list.
  using BooleanPolicyCallback = std::function<void(bool)>;
  using IntegerPolicyCallback = std::function<void(int)>;
  using StringPolicyCallback = std::function<void(const std::string&)>;
  using StringListPolicyCallback =
      std::function<void(const std::vector<std::string>&)>;

  // Some logical grouping of policy encoding.
  void EncodeLoginPolicies(
      enterprise_management::ChromeDeviceSettingsProto* policy) const;
  void EncodeNetworkPolicies(
      enterprise_management::ChromeDeviceSettingsProto* policy) const;
  void EncodeAutoUpdatePolicies(
      enterprise_management::ChromeDeviceSettingsProto* policy) const;
  void EncodeAccessibilityPolicies(
      enterprise_management::ChromeDeviceSettingsProto* policy) const;
  void EncodeGenericPolicies(
      enterprise_management::ChromeDeviceSettingsProto* policy) const;

  // Boolean policies.
  void EncodeBoolean(const char* policy_name,
                     const BooleanPolicyCallback& set_policy) const;
  // Integer policies.
  void EncodeInteger(const char* policy_name,
                     const IntegerPolicyCallback& set_policy) const;
  // Integer in range policies.
  void EncodeIntegerInRange(const char* policy_name,
                            int range_min,
                            int range_max,
                            const IntegerPolicyCallback& set_policy) const;
  // String policies.
  void EncodeString(const char* policy_name,
                    const StringPolicyCallback& set_policy) const;

  // String list policies are a little different. Unlike the basic types they
  // are not stored as registry value, but as registry key with values 1, 2, ...
  // for the entries.
  void EncodeStringList(const char* policy_name,
                        const StringListPolicyCallback& set_policy) const;

  // Prints out an error message if the |policy_name| is contained in the
  // registry dictionary. Use this for unsupported policies.
  void HandleUnsupported(const char* policy_name) const;

 private:
  const RegistryDict* dict_ = nullptr;
  bool log_policy_values_ = false;
};

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_DEVICE_POLICY_ENCODER_H_
