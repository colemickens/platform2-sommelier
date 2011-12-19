// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_service.h"

#include <string>

#include <base/logging.h>
#include <base/message_loop_proxy.h>
#include <base/stl_util-inl.h>
#include <base/synchronization/waitable_event.h>
#include <base/task.h>

#include "login_manager/device_management_backend.pb.h"
#include "login_manager/nss_util.h"
#include "login_manager/owner_key.h"
#include "login_manager/policy_store.h"

namespace em = enterprise_management;

namespace login_manager {

PolicyService::Error::Error()
    : code_(CHROMEOS_LOGIN_ERROR_DECODE_FAIL) {
}

PolicyService::Error::Error(ChromeOSLoginError code,
                            const std::string& message)
    : code_(code),
      message_(message) {
}

void PolicyService::Error::Set(ChromeOSLoginError code,
                               const std::string& message) {
  code_ = code;
  message_ = message;
}

PolicyService::Completion::~Completion() {
}

PolicyService::Delegate::~Delegate() {
}

PolicyService::PolicyService(
    PolicyStore* policy_store,
    OwnerKey* policy_key,
    const scoped_refptr<base::MessageLoopProxy>& main_loop)
    : policy_store_(policy_store),
      policy_key_(policy_key),
      main_loop_(main_loop),
      delegate_(NULL) {
}

PolicyService::~PolicyService() {
}

bool PolicyService::Initialize() {
  bool ignored = true;
  return DoInitialize(&ignored);
}

bool PolicyService::Store(const uint8* policy_blob,
                          uint32 len,
                          Completion* completion,
                          int flags) {
  em::PolicyFetchResponse policy;
  if (!policy.ParseFromArray(policy_blob, len) ||
      !policy.has_policy_data() ||
      !policy.has_policy_data_signature()) {
    const char msg[] = "Unable to parse policy protobuf.";
    LOG(ERROR) << msg;
    completion->Failure(Error(CHROMEOS_LOGIN_ERROR_DECODE_FAIL, msg));
    return FALSE;
  }

  return StorePolicy(policy, completion, flags);
}

bool PolicyService::Retrieve(std::vector<uint8>* policy_blob) {
  const em::PolicyFetchResponse& policy = store()->Get();
  policy_blob->resize(policy.ByteSize());
  uint8* start = vector_as_array(policy_blob);
  uint8* end = policy.SerializeWithCachedSizesToArray(start);
  return (end - start >= 0 &&
          static_cast<size_t>(end - start) == policy_blob->size());
}

bool PolicyService::PersistPolicySync() {
  bool status = store()->Persist();
  OnPolicyPersisted(NULL, status);
  return status;
}

bool PolicyService::DoInitialize(bool* policy_success) {
  DCHECK(policy_success);
  bool key_load_failed = false;
  if (!key()->PopulateFromDiskIfPossible()) {
    LOG(ERROR) << "Failed to load policy key from disk.";
    key_load_failed = true;
  }

  *policy_success = store()->LoadOrCreate();
  if (!*policy_success)  // Non-fatal, so log, and keep going.
    LOG(ERROR) << "Failed to load policy data, continuing anyway.";

  return !key_load_failed;  // Key load failure is fatal.
}

void PolicyService::PersistKey() {
  main_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PolicyService::PersistKeyOnLoop));
}

void PolicyService::PersistPolicy() {
  main_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PolicyService::PersistPolicyOnLoop,
                        static_cast<Completion*>(NULL)));
}

void PolicyService::PersistPolicyWithCompletion(Completion* completion) {
  main_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PolicyService::PersistPolicyOnLoop,
                        completion));
}

bool PolicyService::StorePolicy(const em::PolicyFetchResponse& policy,
                                Completion* completion,
                                int flags) {
  // Determine if the policy has pushed a new owner key and, if so, set it.
  if (policy.has_new_public_key() && !key()->Equals(policy.new_public_key())) {
    // The policy contains a new key, and it is different from |key_|.
    std::vector<uint8> der;
    NssUtil::BlobFromBuffer(policy.new_public_key(), &der);

    bool installed = false;
    if (key()->IsPopulated()) {
      if (policy.has_new_public_key_signature() && (flags & KEY_ROTATE)) {
        // Graceful key rotation.
        LOG(INFO) << "Attempting policy key rotation.";
        std::vector<uint8> sig;
        NssUtil::BlobFromBuffer(policy.new_public_key_signature(), &sig);
        installed = key()->Rotate(der, sig);
      }
    } else if (flags & KEY_INSTALL_NEW) {
      LOG(INFO) << "Attempting to install new policy key.";
      installed = key()->PopulateFromBuffer(der);
    }
    if (!installed && (flags & KEY_CLOBBER)) {
      LOG(INFO) << "Clobbering existing policy key.";
      installed = key()->ClobberCompromisedKey(der);
    }

    if (!installed) {
      const char msg[] = "Failed to install policy key!";
      LOG(ERROR) << msg;
      completion->Failure(Error(CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY, msg));
      return false;
    }

    // If here, need to persist the key just loaded into memory to disk.
    PersistKey();
  }

  // Validate signature on policy and persist to disk.
  const std::string& data(policy.policy_data());
  const std::string& sig(policy.policy_data_signature());
  if (!key()->Verify(reinterpret_cast<const uint8*>(data.c_str()),
                     data.size(),
                     reinterpret_cast<const uint8*>(sig.c_str()),
                     sig.size())) {
    const char msg[] = "Signature could not be verified.";
    LOG(ERROR) << msg;
    completion->Failure(Error(CHROMEOS_LOGIN_ERROR_VERIFY_FAIL, msg));
    return false;
  }

  store()->Set(policy);
  PersistPolicyWithCompletion(completion);
  return true;
}

void PolicyService::PersistKeyOnLoop() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  OnKeyPersisted(key()->Persist());
}

void PolicyService::PersistPolicyOnLoop(Completion* completion) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  OnPolicyPersisted(completion, store()->Persist());
}

void PolicyService::OnKeyPersisted(bool status) {
  if (status)
    LOG(INFO) << "Persisted policy key to disk.";
  else
    LOG(ERROR) << "Failed to persist policy key to disk.";
  if (delegate_)
    delegate_->OnKeyPersisted(status);
}

void PolicyService::OnPolicyPersisted(Completion* completion, bool status) {
  if (status) {
    LOG(INFO) << "Persisted policy to disk.";
    if (completion)
      completion->Success();
  } else {
    std::string msg = "Failed to persist policy to disk.";
    LOG(ERROR) << msg;
    if (completion)
      completion->Failure(Error(CHROMEOS_LOGIN_ERROR_ENCODE_FAIL, msg));
  }

  if (delegate_)
    delegate_->OnPolicyPersisted(status);
}

}  // namespace login_manager
