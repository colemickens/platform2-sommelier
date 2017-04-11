// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_service.h"

#include <stdint.h>

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/synchronization/waitable_event.h>
#include <brillo/message_loops/message_loop.h>
#include <chromeos/dbus/service_constants.h>

#include "bindings/device_management_backend.pb.h"
#include "login_manager/nss_util.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_store.h"
#include "login_manager/system_utils.h"

namespace em = enterprise_management;

namespace login_manager {

PolicyService::Error::Error() : code_(dbus_error::kNone) {
}

PolicyService::Error::Error(const std::string& code, const std::string& message)
    : code_(code), message_(message) {
}

void PolicyService::Error::Set(const std::string& code,
                               const std::string& message) {
  code_ = code;
  message_ = message;
}

PolicyService::Delegate::~Delegate() {
}

PolicyService::PolicyService(
    std::unique_ptr<PolicyStore> policy_store,
    PolicyKey* policy_key)
    : policy_store_(std::move(policy_store)),
      policy_key_(policy_key),
      delegate_(NULL),
      weak_ptr_factory_(this) {
}

PolicyService::~PolicyService() {
  weak_ptr_factory_.InvalidateWeakPtrs();  // Must remain at top of destructor.
}

bool PolicyService::Store(const uint8_t* policy_blob,
                          uint32_t len,
                          const Completion& completion,
                          int key_flags,
                          SignatureCheck signature_check) {
  em::PolicyFetchResponse policy;
  if (!policy.ParseFromArray(policy_blob, len) || !policy.has_policy_data()) {
    static const char msg[] = "Unable to parse policy protobuf.";
    LOG(ERROR) << msg;
    Error error(dbus_error::kSigDecodeFail, msg);
    completion.Run(error);
    return false;
  }

  return StorePolicy(policy, completion, key_flags, signature_check);
}

bool PolicyService::Retrieve(std::vector<uint8_t>* policy_blob) {
  const em::PolicyFetchResponse& policy = store()->Get();
  policy_blob->resize(policy.ByteSize());
  uint8_t* start = policy_blob->data();
  uint8_t* end = policy.SerializeWithCachedSizesToArray(start);
  return (static_cast<size_t>(end - start) == policy_blob->size());
}

void PolicyService::PersistPolicy(const Completion& completion) {
  const bool success = store()->Persist();
  OnPolicyPersisted(completion,
                    success ? dbus_error::kNone : dbus_error::kSigEncodeFail);
}

void PolicyService::PostPersistKeyTask() {
  brillo::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PolicyService::PersistKey, weak_ptr_factory_.GetWeakPtr()));
}

void PolicyService::PostPersistPolicyTask(const Completion& completion) {
  brillo::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PolicyService::PersistPolicy,
                 weak_ptr_factory_.GetWeakPtr(),
                 completion));
}

bool PolicyService::StorePolicy(const em::PolicyFetchResponse& policy,
                                const Completion& completion,
                                int key_flags,
                                SignatureCheck signature_check) {
  if (signature_check == SignatureCheck::kDisabled) {
    store()->Set(policy);
    PostPersistPolicyTask(completion);
    return true;
  }

  // Determine if the policy has pushed a new owner key and, if so, set it.
  if (policy.has_new_public_key() && !key()->Equals(policy.new_public_key())) {
    // The policy contains a new key, and it is different from |key_|.
    std::vector<uint8_t> der;
    NssUtil::BlobFromBuffer(policy.new_public_key(), &der);

    bool installed = false;
    if (key()->IsPopulated()) {
      if (policy.has_new_public_key_signature() && (key_flags & KEY_ROTATE)) {
        // Graceful key rotation.
        LOG(INFO) << "Attempting policy key rotation.";
        std::vector<uint8_t> sig;
        NssUtil::BlobFromBuffer(policy.new_public_key_signature(), &sig);
        installed = key()->Rotate(der, sig);
      }
    } else if (key_flags & KEY_INSTALL_NEW) {
      LOG(INFO) << "Attempting to install new policy key.";
      installed = key()->PopulateFromBuffer(der);
    }
    if (!installed && (key_flags & KEY_CLOBBER)) {
      LOG(INFO) << "Clobbering existing policy key.";
      installed = key()->ClobberCompromisedKey(der);
    }

    if (!installed) {
      static const char msg[] = "Failed to install policy key!";
      LOG(ERROR) << msg;
      Error error(dbus_error::kPubkeySetIllegal, msg);
      completion.Run(error);
      return false;
    }

    // If here, need to persist the key just loaded into memory to disk.
    PostPersistKeyTask();
  }

  // Validate signature on policy and persist to disk.
  const std::string& data(policy.policy_data());
  const std::string& sig(policy.policy_data_signature());
  if (!key()->Verify(reinterpret_cast<const uint8_t*>(data.c_str()),
                     data.size(),
                     reinterpret_cast<const uint8_t*>(sig.c_str()),
                     sig.size())) {
    static const char msg[] = "Signature could not be verified.";
    LOG(ERROR) << msg;
    Error error(dbus_error::kVerifyFail, msg);
    completion.Run(error);
    return false;
  }

  store()->Set(policy);
  PostPersistPolicyTask(completion);
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

void PolicyService::OnPolicyPersisted(const Completion& completion,
                                      const std::string& dbus_error_type) {
  std::string msg;
  if (dbus_error_type == dbus_error::kNone) {
    LOG(INFO) << "Persisted policy to disk.";
  } else {
    msg = "Failed to persist policy to disk.";
    LOG(ERROR) << msg << dbus_error_type;
  }

  if (!completion.is_null())
    completion.Run(Error(dbus_error_type, msg));

  if (delegate_)
    delegate_->OnPolicyPersisted(dbus_error_type == dbus_error::kNone);
}

void PolicyService::PersistKey() {
  OnKeyPersisted(key()->Persist());
}

}  // namespace login_manager
