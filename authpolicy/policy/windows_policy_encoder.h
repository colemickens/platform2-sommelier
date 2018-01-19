// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_WINDOWS_POLICY_ENCODER_H_
#define AUTHPOLICY_POLICY_WINDOWS_POLICY_ENCODER_H_

namespace authpolicy {
namespace protos {
class WindowsPolicy;
}
}  // namespace authpolicy

namespace base {
class Value;
}

namespace policy {

class RegistryDict;

// Private helper class used to convert a RegistryDict into a WindowsPolicy
// object. Don't include directly, use |preg_policy_encoder.h| instead.
class WindowsPolicyEncoder {
 public:
  explicit WindowsPolicyEncoder(const RegistryDict* dict);

  // Toggles logging of policy values.
  void LogPolicyValues(bool enabled) { log_policy_values_ = enabled; }

  // Extracts all interesting Windows policies from |dict_| and puts them into
  // |policy|. |dict_| is a collection of Windows policies under the registry
  // key |kKeyWindows|.
  void EncodePolicy(authpolicy::protos::WindowsPolicy* policy) const;

 private:
  using PolicyEncoder = void(authpolicy::protos::WindowsPolicy* policy,
                             const base::Value* value);

  // Checks whether |dict_| contains |policy_name| and uses the custom |encoder|
  // function to write the value into |policy|.
  void EncodeSinglePolicy(authpolicy::protos::WindowsPolicy* policy,
                          const char* policy_name,
                          PolicyEncoder encoder) const;

  const RegistryDict* dict_ = nullptr;
  bool log_policy_values_ = false;
};

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_WINDOWS_POLICY_ENCODER_H_
