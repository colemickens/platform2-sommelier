// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/device_policy.h"

#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>

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
  if (!file_util::ReadFileToString(policy_path_, &policy_) || policy_.empty()) {
    PLOG(ERROR) << "Could not read policy off disk: ";
    return false;
  }
  return true;
}

const std::string&  DevicePolicy::Get() {
  return policy_;
}

bool DevicePolicy::Persist() {
  SystemUtils utils;
  return utils.AtomicFileWrite(policy_path_, policy_.c_str(), policy_.length());
}

void DevicePolicy::Set(const std::string& policy) {
  policy_ = policy;
}

}  // namespace login_manager
