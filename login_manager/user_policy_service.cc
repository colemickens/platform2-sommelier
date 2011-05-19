// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "user_policy_service.h"

#include <base/logging.h>
#include <base/message_loop_proxy.h>

#include "login_manager/device_management_backend.pb.h"
#include "login_manager/owner_key.h"
#include "login_manager/policy_store.h"

namespace em = enterprise_management;

namespace login_manager {

UserPolicyService::UserPolicyService(
    PolicyStore* policy_store,
    OwnerKey* policy_key,
    const scoped_refptr<base::MessageLoopProxy>& main_loop)
    : PolicyService(policy_store, policy_key, main_loop) {
}

UserPolicyService::~UserPolicyService() {
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
    completion->Failure(Error(CHROMEOS_LOGIN_ERROR_DECODE_FAIL, msg));
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

}  // namespace login_manager
