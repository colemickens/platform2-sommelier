// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>

#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/system_utils.h"

namespace login_manager {
// static
const char DevicePolicy::kDefaultPath[] = "/var/lib/whitelist/policy";

DevicePolicy::DevicePolicy(const FilePath& policy_path)
    : policy_path_(policy_path) {
}

DevicePolicy::~DevicePolicy() {
}

bool DevicePolicy::LoadOrCreate() {
  if (!file_util::PathExists(policy_path_))
    return true;
  std::string polstr;
  if (!file_util::ReadFileToString(policy_path_, &polstr) || polstr.empty()) {
    PLOG(ERROR) << "Could not read policy off disk";
    return false;
  }
  if (!policy_.ParseFromString(polstr)) {
    LOG(ERROR) << "Policy on disk could not be parsed!";
    return false;
  }
  return true;
}

bool DevicePolicy::Get(std::string* output) const {
  return policy_.SerializeToString(output);
}

bool DevicePolicy::Persist() {
  SystemUtils utils;
  std::string polstr;
  if (!policy_.SerializeToString(&polstr)) {
    LOG(ERROR) << "Could not be serialize policy!";
    return false;
  }
  return utils.AtomicFileWrite(policy_path_, polstr.c_str(), polstr.length());
}

void DevicePolicy::Set(
    const enterprise_management::PolicyFetchResponse& policy) {
  policy_.Clear();
  // This can only fail if |policy| and |policy_| are different types.
  policy_.CheckTypeAndMergeFrom(policy);
}

}  // namespace login_manager
