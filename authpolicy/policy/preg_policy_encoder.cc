// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/policy/preg_policy_encoder.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"

#include "authpolicy/policy/device_policy_encoder.h"
#include "authpolicy/policy/policy_encoder_helper.h"
#include "authpolicy/policy/proto/cloud_policy.pb.h"
#include "authpolicy/policy/registry_dict.h"
#include "authpolicy/policy/user_policy_encoder.h"

namespace em = enterprise_management;

namespace {

const char kRecommendedKey[] = "Recommended";

}  // namespace

namespace policy {

bool ParsePRegFilesIntoUserPolicy(const std::vector<base::FilePath>& preg_files,
                                  brillo::ErrorPtr* error,
                                  em::CloudPolicySettings* policy) {
  DCHECK(policy);

  RegistryDict merged_mandatory_dict;
  RegistryDict merged_recommended_dict;
  for (const base::FilePath& preg_file : preg_files) {
    RegistryDict mandatory_dict;
    if (!helper::LoadPRegFile(preg_file, error, &mandatory_dict))
      return false;

    // Recommended policies are stored in their own registry key. This can be
    // nullptr if there is no recommended policy.
    std::unique_ptr<RegistryDict> recommended_dict =
        mandatory_dict.RemoveKey(kRecommendedKey);

    // Merge into cumulative dicts. The rhs overwrites same policies in the lhs.
    merged_mandatory_dict.Merge(mandatory_dict);
    if (recommended_dict)
      merged_recommended_dict.Merge(*recommended_dict);
  }

  // Convert recommended policies first. If a policy is both recommended and
  // mandatory, it will be overwritten to be mandatory below.
  {
    UserPolicyEncoder enc(&merged_recommended_dict, POLICY_LEVEL_RECOMMENDED);
    enc.EncodeUserPolicy(policy);
  }

  {
    UserPolicyEncoder enc(&merged_mandatory_dict, POLICY_LEVEL_MANDATORY);
    enc.EncodeUserPolicy(policy);
  }

  return true;
}

bool ParsePRegFilesIntoDevicePolicy(
    const std::vector<base::FilePath>& preg_files,
    brillo::ErrorPtr* error,
    em::ChromeDeviceSettingsProto* policy) {
  DCHECK(policy);

  RegistryDict policy_dict;
  for (const base::FilePath& preg_file : preg_files) {
    if (!helper::LoadPRegFile(preg_file, error, &policy_dict))
      return false;
  }

  DevicePolicyEncoder encoder(&policy_dict);
  encoder.EncodeDevicePolicy(policy);

  return true;
}

}  // namespace policy
