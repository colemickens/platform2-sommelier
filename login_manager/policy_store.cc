// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_store.h"

#include <base/files/file_util.h>
#include <base/logging.h>

#include "login_manager/login_metrics.h"
#include "login_manager/system_utils_impl.h"

namespace login_manager {
// static
const char PolicyStore::kPrefsFileName[] = "preferences";

PolicyStore::PolicyStore(const base::FilePath& policy_path)
    : policy_path_(policy_path) {
}

PolicyStore::~PolicyStore() {
}

bool PolicyStore::DefunctPrefsFilePresent() {
  return base::PathExists(policy_path_.DirName().Append(kPrefsFileName));
}

bool PolicyStore::LoadOrCreate() {
  if (!base::PathExists(policy_path_)) {
    cached_policy_data_.clear();
    return true;
  }

  std::string polstr;
  if (!base::ReadFileToString(policy_path_, &polstr) || polstr.empty()) {
    PLOG(ERROR) << "Could not read policy off disk at " << policy_path_.value();
    cached_policy_data_.clear();
    return false;
  }
  if (!policy_.ParseFromString(polstr)) {
    LOG(ERROR) << "Policy on disk could not be parsed and will be deleted!";
    base::DeleteFile(policy_path_, false);
    cached_policy_data_.clear();
    return false;
  }
  cached_policy_data_ = polstr;

  return true;
}

const enterprise_management::PolicyFetchResponse& PolicyStore::Get() const {
  return policy_;
}

bool PolicyStore::Persist() {
  SystemUtilsImpl utils;
  std::string polstr;
  if (!policy_.SerializeToString(&polstr)) {
    LOG(ERROR) << "Could not serialize policy!";
    return false;
  }

  // Skip writing to the file if the contents of policy data haven't been
  // changed.
  if (cached_policy_data_ == polstr)
    return true;

  if (!utils.AtomicFileWrite(policy_path_, polstr))
    return false;

  LOG(INFO) << "Persisted policy to disk.";
  cached_policy_data_ = polstr;
  return true;
}

void PolicyStore::Set(
    const enterprise_management::PolicyFetchResponse& policy) {
  policy_.Clear();
  // This can only fail if |policy| and |policy_| are different types.
  policy_.CheckTypeAndMergeFrom(policy);
}

}  // namespace login_manager
