// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "user_policy_service.h"

#include <sys/stat.h>

#include <vector>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/message_loop_proxy.h>

#include "login_manager/device_management_backend.pb.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_store.h"
#include "login_manager/system_utils.h"

namespace em = enterprise_management;

namespace login_manager {

UserPolicyService::UserPolicyService(
    scoped_ptr<PolicyStore> policy_store,
    scoped_ptr<PolicyKey> policy_key,
    const FilePath& key_copy_path,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    SystemUtils* system_utils)
    : PolicyService(policy_store.Pass(), policy_key.get(), main_loop),
      scoped_policy_key_(policy_key.Pass()),
      key_copy_path_(key_copy_path),
      system_utils_(system_utils) {
}

UserPolicyService::~UserPolicyService() {
}

void UserPolicyService::PersistKeyCopy() {
  // Create a copy at |key_copy_path_| that is readable by chronos.
  if (key_copy_path_.empty())
    return;
  if (scoped_policy_key_->IsPopulated()) {
    FilePath dir(key_copy_path_.DirName());
    file_util::CreateDirectory(dir);
    mode_t mode = S_IRWXU | S_IXGRP | S_IXOTH;
    chmod(dir.value().c_str(), mode);

    const std::vector<uint8>& key = scoped_policy_key_->public_key_der();
    system_utils_->AtomicFileWrite(key_copy_path_,
                                   reinterpret_cast<const char*>(&key[0]),
                                   key.size());
    mode = S_IRUSR | S_IRGRP | S_IROTH;
    chmod(key_copy_path_.value().c_str(), mode);
  } else {
    // Remove the key if it has been cleared.
    system_utils_->RemoveFile(key_copy_path_);
  }
}

bool UserPolicyService::Store(const uint8* policy_blob,
                              uint32 len,
                              Completion* completion,
                              int flags) {
  em::PolicyFetchResponse policy;
  em::PolicyData policy_data;
  if (!policy.ParseFromArray(policy_blob, len) ||
      !policy.has_policy_data() ||
      !policy_data.ParseFromString(policy.policy_data())) {
    const char msg[] = "Unable to parse policy protobuf.";
    LOG(ERROR) << msg;
    Error error(CHROMEOS_LOGIN_ERROR_DECODE_FAIL, msg);
    completion->Failure(error);
    return false;
  }

  // Allow to switch to unmanaged state even if no signature is present.
  if (policy_data.state() == em::PolicyData::UNMANAGED &&
      !policy.has_policy_data_signature()) {
    // Also clear the key.
    if (key()->IsPopulated()) {
      key()->ClobberCompromisedKey(std::vector<uint8>());
      PersistKey();
    }

    store()->Set(policy);
    PersistPolicyWithCompletion(completion);
    return true;
  }

  return PolicyService::StorePolicy(policy, completion, flags);
}

void UserPolicyService::OnKeyPersisted(bool status) {
  if (status)
    PersistKeyCopy();
  // Only notify the delegate after writing the copy, so that chrome can find
  // the file after being notified that the key is ready.
  PolicyService::OnKeyPersisted(status);
}

}  // namespace login_manager
