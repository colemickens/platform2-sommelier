// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/windows_policy_manager.h"

#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>

#include <string>
#include <utility>

#include "bindings/authpolicy_containers.pb.h"

namespace {
// Size limit when loading the policy file (256 kb).
const size_t kPolicySizeLimit = 256 * 1024;
}  // namespace

namespace authpolicy {

WindowsPolicyManager::WindowsPolicyManager(const base::FilePath& policy_path)
    : policy_path_(policy_path) {}

ErrorType WindowsPolicyManager::LoadFromDisk() {
  // Missing policy is fine, might have not been set yet.
  if (!base::PathExists(policy_path_))
    return ERROR_NONE;

  // Read Windows policy to string.
  std::string policy_blob;
  if (!base::ReadFileToStringWithMaxSize(policy_path_, &policy_blob,
                                         kPolicySizeLimit)) {
    PLOG(ERROR) << "Failed to read Windows policy from "
                << policy_path_.value();
    return ERROR_LOCAL_IO;
  }

  // Parse string to proto.
  auto policy = std::make_unique<protos::WindowsPolicy>();
  if (!policy->ParseFromString(policy_blob)) {
    LOG(ERROR) << "Failed to parse Windows policy from string";
    return ERROR_LOCAL_IO;
  }

  // Policy successfully loaded.
  policy_ = std::move(policy);
  return ERROR_NONE;
}

ErrorType WindowsPolicyManager::UpdateAndSaveToDisk(
    std::unique_ptr<protos::WindowsPolicy> policy) {
  // Serialize proto to string.
  std::string policy_blob;
  if (!policy->SerializeToString(&policy_blob)) {
    LOG(ERROR) << "Failed to serialize Windows policy";
    return ERROR_LOCAL_IO;
  }

  // Write atomically for more safety.
  if (!base::ImportantFileWriter::WriteFileAtomically(policy_path_,
                                                      policy_blob)) {
    PLOG(ERROR) << "Failed to write Windows policy to " << policy_path_.value();
    return ERROR_LOCAL_IO;
  }

  // Reduce permissions to the minimum.
  constexpr int mode =
      base::FILE_PERMISSION_READ_BY_USER | base::FILE_PERMISSION_WRITE_BY_USER;
  if (!base::SetPosixFilePermissions(policy_path_, mode)) {
    PLOG(ERROR) << "Failed to set permissions on " << policy_path_.value();
    return ERROR_LOCAL_IO;
  }

  policy_ = std::move(policy);
  return ERROR_NONE;
}

bool WindowsPolicyManager::ClearPolicyForTesting() {
  policy_.reset();
  return base::DeleteFile(policy_path_, false /* recursive */);
}

}  // namespace authpolicy
