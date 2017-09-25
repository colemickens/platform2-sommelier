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
#include "login_manager/blob_util.h"
#include "login_manager/dbus_util.h"
#include "login_manager/nss_util.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_store.h"
#include "login_manager/resilient_policy_store.h"
#include "login_manager/system_utils.h"
#include "login_manager/validator_utils.h"

namespace em = enterprise_management;

namespace login_manager {

PolicyNamespace MakeChromePolicyNamespace() {
  return std::make_pair(POLICY_DOMAIN_CHROME, std::string());
}

constexpr char PolicyService::kChromePolicyFileName[] = "policy";
constexpr char PolicyService::kExtensionsPolicyFileNamePrefix[] =
    "policy_extension_id_";
constexpr char PolicyService::kSignInExtensionsPolicyFileNamePrefix[] =
    "policy_signin_extension_id_";

PolicyService::PolicyService(const base::FilePath& policy_dir,
                             PolicyKey* policy_key,
                             LoginMetrics* metrics,
                             bool resilient_chrome_policy_store)
    : metrics_(metrics),
      policy_dir_(policy_dir),
      policy_key_(policy_key),
      resilient_chrome_policy_store_(resilient_chrome_policy_store),
      delegate_(NULL),
      weak_ptr_factory_(this) {}

PolicyService::~PolicyService() = default;

bool PolicyService::Store(const PolicyNamespace& ns,
                          const std::vector<uint8_t>& policy_blob,
                          int key_flags,
                          SignatureCheck signature_check,
                          const Completion& completion) {
  em::PolicyFetchResponse policy;
  if (!policy.ParseFromArray(policy_blob.data(), policy_blob.size()) ||
      !policy.has_policy_data()) {
    constexpr char kMessage[] = "Unable to parse policy protobuf.";
    LOG(ERROR) << kMessage;
    completion.Run(CreateError(dbus_error::kSigDecodeFail, kMessage));
    return false;
  }

  return StorePolicy(ns, policy, key_flags, signature_check, completion);
}

bool PolicyService::Retrieve(const PolicyNamespace& ns,
                             std::vector<uint8_t>* policy_blob) {
  *policy_blob = SerializeAsBlob(GetOrCreateStore(ns)->Get());
  return true;
}

void PolicyService::PersistPolicy(const PolicyNamespace& ns,
                                  const Completion& completion) {
  const bool success = GetOrCreateStore(ns)->Persist();
  OnPolicyPersisted(completion,
                    success ? dbus_error::kNone : dbus_error::kSigEncodeFail);
}

void PolicyService::PersistAllPolicy() {
  for (const auto& kv : policy_stores_)
    kv.second->Persist();
}

PolicyStore* PolicyService::GetOrCreateStore(const PolicyNamespace& ns) {
  PolicyStoreMap::const_iterator iter = policy_stores_.find(ns);
  if (iter != policy_stores_.end())
    return iter->second.get();

  bool resilient =
      (ns == MakeChromePolicyNamespace() && resilient_chrome_policy_store_);
  std::unique_ptr<PolicyStore> store;
  if (resilient)
    store = std::make_unique<ResilientPolicyStore>(GetPolicyPath(ns), metrics_);
  else
    store = std::make_unique<PolicyStore>(GetPolicyPath(ns));

  store->EnsureLoadedOrCreated();
  PolicyStore* policy_store_ptr = store.get();
  policy_stores_[ns] = std::move(store);
  return policy_store_ptr;
}

void PolicyService::SetStoreForTesting(const PolicyNamespace& ns,
                                       std::unique_ptr<PolicyStore> store) {
  policy_stores_[ns] = std::move(store);
}

void PolicyService::PostPersistKeyTask() {
  brillo::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PolicyService::PersistKey, weak_ptr_factory_.GetWeakPtr()));
}

void PolicyService::PostPersistPolicyTask(const PolicyNamespace& ns,
                                          const Completion& completion) {
  brillo::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&PolicyService::PersistPolicy,
                            weak_ptr_factory_.GetWeakPtr(), ns, completion));
}

bool PolicyService::StorePolicy(const PolicyNamespace& ns,
                                const em::PolicyFetchResponse& policy,
                                int key_flags,
                                SignatureCheck signature_check,
                                const Completion& completion) {
  if (signature_check == SignatureCheck::kDisabled) {
    GetOrCreateStore(ns)->Set(policy);
    PostPersistPolicyTask(ns, completion);
    return true;
  }

  // Determine if the policy has pushed a new owner key and, if so, set it.
  if (policy.has_new_public_key() && !key()->Equals(policy.new_public_key())) {
    // The policy contains a new key, and it is different from |key_|.
    std::vector<uint8_t> der = StringToBlob(policy.new_public_key());

    bool installed = false;
    if (key()->IsPopulated()) {
      if (policy.has_new_public_key_signature() && (key_flags & KEY_ROTATE)) {
        // Graceful key rotation.
        LOG(INFO) << "Attempting policy key rotation.";
        installed =
            key()->Rotate(der, StringToBlob(policy.new_public_key_signature()));
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
      constexpr char kMessage[] = "Failed to install policy key!";
      LOG(ERROR) << kMessage;
      completion.Run(CreateError(dbus_error::kPubkeySetIllegal, kMessage));
      return false;
    }

    // If here, need to persist the key just loaded into memory to disk.
    PostPersistKeyTask();
  }

  // Validate signature on policy and persist to disk.
  if (!key()->Verify(StringToBlob(policy.policy_data()),
                     StringToBlob(policy.policy_data_signature()))) {
    constexpr char kMessage[] = "Signature could not be verified.";
    LOG(ERROR) << kMessage;
    completion.Run(CreateError(dbus_error::kVerifyFail, kMessage));
    return false;
  }

  GetOrCreateStore(ns)->Set(policy);
  PostPersistPolicyTask(ns, completion);
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
                                      const std::string& dbus_error_code) {
  brillo::ErrorPtr error;
  if (dbus_error_code != dbus_error::kNone) {
    constexpr char kMessage[] = "Failed to persist policy to disk.";
    LOG(ERROR) << kMessage << ": " << dbus_error_code;
    error = CreateError(dbus_error_code, kMessage);
  }

  if (!completion.is_null())
    completion.Run(std::move(error));
  else
    error.reset();

  if (delegate_)
    delegate_->OnPolicyPersisted(dbus_error_code == dbus_error::kNone);
}

void PolicyService::PersistKey() {
  OnKeyPersisted(key()->Persist());
}

base::FilePath PolicyService::GetPolicyPath(const PolicyNamespace& ns) {
  // If the store has already been already created, return the store's path.
  PolicyStoreMap::const_iterator iter = policy_stores_.find(ns);
  if (iter != policy_stores_.end())
    return iter->second->policy_path();

  const PolicyDomain& domain = ns.first;
  const std::string& component_id = ns.second;
  switch (domain) {
    case POLICY_DOMAIN_CHROME:
      return policy_dir_.AppendASCII(kChromePolicyFileName);
    case POLICY_DOMAIN_EXTENSIONS:
      // Double-check extension id (should have already been checked before).
      CHECK(ValidateExtensionId(component_id));
      return policy_dir_.AppendASCII(kExtensionsPolicyFileNamePrefix +
                                     component_id);
    case POLICY_DOMAIN_SIGNIN_EXTENSIONS:
      // Double-check extension id (should have already been checked before).
      CHECK(ValidateExtensionId(component_id));
      return policy_dir_.AppendASCII(kSignInExtensionsPolicyFileNamePrefix +
                                     component_id);
  }
}

}  // namespace login_manager
