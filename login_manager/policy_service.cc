// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_service.h"

#include <stdint.h>

#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/stl_util.h>
#include <base/synchronization/waitable_event.h>

#include "bindings/device_management_backend.pb.h"
#include "login_manager/dbus_error_types.h"
#include "login_manager/nss_util.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_store.h"
#include "login_manager/system_utils.h"

namespace em = enterprise_management;

namespace login_manager {

PolicyService::Error::Error() : code_(dbus_error::kSigDecodeFail) {
}

PolicyService::Error::Error(const std::string& code, const std::string& message)
    : code_(code), message_(message) {
}

void PolicyService::Error::Set(const std::string& code,
                               const std::string& message) {
  code_ = code;
  message_ = message;
}

PolicyService::Completion::~Completion() {
}

PolicyService::Delegate::~Delegate() {
}

PolicyService::PolicyService(
    scoped_ptr<PolicyStore> policy_store,
    PolicyKey* policy_key,
    const scoped_refptr<base::MessageLoopProxy>& main_loop)
    : policy_store_(policy_store.Pass()),
      policy_key_(policy_key),
      main_loop_(main_loop),
      delegate_(NULL),
      weak_ptr_factory_(this) {
}

PolicyService::~PolicyService() {
  weak_ptr_factory_.InvalidateWeakPtrs();  // Must remain at top of destructor.
}

bool PolicyService::Store(const uint8_t* policy_blob,
                          uint32_t len,
                          Completion* completion,
                          int flags) {
  em::PolicyFetchResponse policy;
  if (!policy.ParseFromArray(policy_blob, len) || !policy.has_policy_data() ||
      !policy.has_policy_data_signature()) {
    const char msg[] = "Unable to parse policy protobuf.";
    LOG(ERROR) << msg;
    Error error(dbus_error::kSigDecodeFail, msg);
    completion->ReportFailure(error);
    return FALSE;
  }

  return StorePolicy(policy, completion, flags);
}

bool PolicyService::Retrieve(std::vector<uint8_t>* policy_blob) {
  const em::PolicyFetchResponse& policy = store()->Get();
  policy_blob->resize(policy.ByteSize());
  uint8_t* start = vector_as_array(policy_blob);
  uint8_t* end = policy.SerializeWithCachedSizesToArray(start);
  return (end - start >= 0 &&
          static_cast<size_t>(end - start) == policy_blob->size());
}

bool PolicyService::PersistPolicySync() {
  bool status = store()->Persist();
  OnPolicyPersisted(NULL, status);
  return status;
}

void PolicyService::PersistKey() {
  main_loop_->PostTask(FROM_HERE,
                       base::Bind(&PolicyService::PersistKeyOnLoop,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PolicyService::PersistPolicy() {
  main_loop_->PostTask(FROM_HERE,
                       base::Bind(&PolicyService::PersistPolicyOnLoop,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  static_cast<Completion*>(NULL)));
}

void PolicyService::PersistPolicyWithCompletion(Completion* completion) {
  main_loop_->PostTask(FROM_HERE,
                       base::Bind(&PolicyService::PersistPolicyOnLoop,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  completion));
}

bool PolicyService::StorePolicy(const em::PolicyFetchResponse& policy,
                                Completion* completion,
                                int flags) {
  // Determine if the policy has pushed a new owner key and, if so, set it.
  if (policy.has_new_public_key() && !key()->Equals(policy.new_public_key())) {
    // The policy contains a new key, and it is different from |key_|.
    std::vector<uint8_t> der;
    NssUtil::BlobFromBuffer(policy.new_public_key(), &der);

    bool installed = false;
    if (key()->IsPopulated()) {
      if (policy.has_new_public_key_signature() && (flags & KEY_ROTATE)) {
        // Graceful key rotation.
        LOG(INFO) << "Attempting policy key rotation.";
        std::vector<uint8_t> sig;
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
      Error error(dbus_error::kPubkeySetIllegal, msg);
      completion->ReportFailure(error);
      return false;
    }

    // If here, need to persist the key just loaded into memory to disk.
    PersistKey();
  }

  // Validate signature on policy and persist to disk.
  const std::string& data(policy.policy_data());
  const std::string& sig(policy.policy_data_signature());
  if (!key()->Verify(reinterpret_cast<const uint8_t*>(data.c_str()),
                     data.size(),
                     reinterpret_cast<const uint8_t*>(sig.c_str()),
                     sig.size())) {
    const char msg[] = "Signature could not be verified.";
    LOG(ERROR) << msg;
    Error error(dbus_error::kVerifyFail, msg);
    completion->ReportFailure(error);
    return false;
  }

  store()->Set(policy);
  PersistPolicyWithCompletion(completion);
  return true;
}

void PolicyService::OnKeyPersisted(bool status) {
  if (status)
    LOG(INFO) << "Persisted policy key to disk.";
  else
    LOG(ERROR) << "Failed to persist policy key to disk.";
  if (delegate_)
    delegate_->OnKeyPersisted(status);
}

void PolicyService::PersistKeyOnLoop() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  OnKeyPersisted(key()->Persist());
}

void PolicyService::PersistPolicyOnLoop(Completion* completion) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  OnPolicyPersisted(completion, store()->Persist());
}

void PolicyService::OnPolicyPersisted(Completion* completion, bool status) {
  if (status) {
    LOG(INFO) << "Persisted policy to disk.";
    if (completion)
      completion->ReportSuccess();
  } else {
    std::string msg = "Failed to persist policy to disk.";
    LOG(ERROR) << msg;
    if (completion) {
      Error error(dbus_error::kSigEncodeFail, msg);
      completion->ReportFailure(error);
    }
  }

  if (delegate_)
    delegate_->OnPolicyPersisted(status);
}

}  // namespace login_manager
