// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_POLICY_PREG_POLICY_ENCODER_H_
#define AUTHPOLICY_POLICY_PREG_POLICY_ENCODER_H_

#include <vector>

#include <dbus/authpolicy/dbus-constants.h>

namespace base {
class FilePath;
}  // namespace base

namespace enterprise_management {
class CloudPolicySettings;
class ChromeDeviceSettingsProto;
}  // namespace enterprise_management

namespace policy {

// Loads the given set of |preg_files| and encodes all user policies into the
// given |policy| blob. Note that user policy can contain mandatory and
// recommended policies. If multiple files f1,...,fN are passed in, policies
// are merged with following rules:
// - Mandatory policies in fn overwrite mandatory policies in fm if n > m.
// - Recommended policies in fn overwrite recommended policies in fm if n > m.
// - Mandatory policies always overwrite recommended policies.
// Thus, a mandatory policy in f1 will overwrite a recommended policy in f3,
// even though f3 has the higher index.
bool ParsePRegFilesIntoUserPolicy(
    const std::vector<base::FilePath>& preg_files,
    enterprise_management::CloudPolicySettings* policy,
    authpolicy::ErrorType* out_error);

// Loads the given set of |preg_files| and encodes all device policies into the
// given |policy| blob. If multiple files f1,...,fN are passed in, policies
// are merged with following rule:
// - Policies in fn overwrite policies in fm if n > m.
bool ParsePRegFilesIntoDevicePolicy(
    const std::vector<base::FilePath>& preg_files,
    enterprise_management::ChromeDeviceSettingsProto* policy,
    authpolicy::ErrorType* out_error);

}  // namespace policy

#endif  // AUTHPOLICY_POLICY_PREG_POLICY_ENCODER_H_
