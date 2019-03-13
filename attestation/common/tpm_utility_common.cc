// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/common/tpm_utility_common.h"

#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <crypto/scoped_openssl_types.h>
#include <crypto/sha2.h>
#include <openssl/rsa.h>

#include "tpm_manager/client/tpm_nvram_dbus_proxy.h"
#include "tpm_manager/client/tpm_ownership_dbus_proxy.h"
#include "tpm_manager/common/tpm_manager_constants.h"

namespace {

const unsigned int kWellKnownExponent = 65537;

// An authorization delegate to manage multiple authorization sessions for a
// single command.

}  // namespace

namespace attestation {

TpmUtilityCommon::TpmUtilityCommon(
    tpm_manager::TpmOwnershipInterface* tpm_owner,
    tpm_manager::TpmNvramInterface* tpm_nvram)
    : tpm_owner_(tpm_owner), tpm_nvram_(tpm_nvram) {}

TpmUtilityCommon::~TpmUtilityCommon() {}

void TpmUtilityCommon::InitializationTask(base::WaitableEvent* completion) {
  CHECK(completion);
  CHECK(tpm_manager_thread_.task_runner()->RunsTasksOnCurrentThread());

  default_tpm_owner_ = std::make_unique<tpm_manager::TpmOwnershipDBusProxy>();
  default_tpm_nvram_ = std::make_unique<tpm_manager::TpmNvramDBusProxy>();
  if (default_tpm_owner_->Initialize()) {
    tpm_owner_ = default_tpm_owner_.get();
  }
  if (default_tpm_nvram_->Initialize()) {
    tpm_nvram_ = default_tpm_nvram_.get();
  }
  completion->Signal();
}

void TpmUtilityCommon::ShutdownTask() {
  tpm_owner_ = nullptr;
  tpm_nvram_ = nullptr;
  default_tpm_owner_.reset(nullptr);
  default_tpm_nvram_.reset(nullptr);
}

bool TpmUtilityCommon::Initialize() {
  if (!tpm_manager_thread_.StartWithOptions(base::Thread::Options(
          base::MessageLoopForIO::TYPE_IO, 0 /* Default stack size. */))) {
    LOG(ERROR) << "Failed to start tpm_manager thread.";
    return false;
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    tpm_manager_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&TpmUtilityCommon::InitializationTask,
                              base::Unretained(this), &event));
    event.Wait();
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    LOG(ERROR) << "Failed to initialize tpm_managerd clients.";
    return false;
  }

  return true;
}

bool TpmUtilityCommon::IsTpmReady() {
  if (!is_ready_) {
    CacheTpmState();
  }
  return is_ready_;
}

template <typename ReplyProtoType, typename MethodType>
void TpmUtilityCommon::SendTpmManagerRequestAndWait(
    const MethodType& method, ReplyProtoType* reply_proto) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::Bind(
      [](ReplyProtoType* target, base::WaitableEvent* completion,
         const ReplyProtoType& reply) {
        *target = reply;
        completion->Signal();
      },
      reply_proto, &event);
  tpm_manager_thread_.task_runner()->PostTask(FROM_HERE,
                                              base::Bind(method, callback));
  event.Wait();
}

bool TpmUtilityCommon::GetEndorsementPassword(std::string* password) {
  if (endorsement_password_.empty()) {
    if (!CacheTpmState()) {
      return false;
    }
    if (endorsement_password_.empty()) {
      LOG(WARNING) << "TPM endorsement password is not available.";
      return false;
    }
  }
  *password = endorsement_password_;
  return true;
}

bool TpmUtilityCommon::GetOwnerPassword(std::string* password) {
  if (owner_password_.empty()) {
    if (!CacheTpmState()) {
      return false;
    }
    if (owner_password_.empty()) {
      LOG(WARNING) << "TPM owner password is not available.";
      return false;
    }
  }
  *password = owner_password_;
  return true;
}

bool TpmUtilityCommon::CacheTpmState() {
  tpm_manager::GetTpmStatusReply tpm_status;
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::GetTpmStatus,
                 base::Unretained(tpm_owner_),
                 tpm_manager::GetTpmStatusRequest()),
      &tpm_status);
  if (tpm_status.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to read TPM state from tpm_managerd.";
    return false;
  }
  is_ready_ = tpm_status.enabled() && tpm_status.owned();
  const auto& local_data = tpm_status.local_data();
  endorsement_password_ = local_data.endorsement_password();
  owner_password_ = local_data.owner_password();
  delegate_blob_ = local_data.owner_delegate().blob();
  delegate_secret_ = local_data.owner_delegate().secret();
  return true;
}

bool TpmUtilityCommon::RemoveOwnerDependency() {
  tpm_manager::RemoveOwnerDependencyReply reply;
  tpm_manager::RemoveOwnerDependencyRequest request;
  request.set_owner_dependency(tpm_manager::kTpmOwnerDependency_Attestation);
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::RemoveOwnerDependency,
                 base::Unretained(tpm_owner_), request),
      &reply);
  if (reply.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(WARNING) << __func__ << ": Failed to remove the dependency.";
    return false;
  }
  return true;
}

crypto::ScopedRSA TpmUtilityCommon::CreateRSAFromRawModulus(
    uint8_t* modulus_buffer, size_t modulus_size) {
  crypto::ScopedRSA rsa(RSA_new());
  if (!rsa.get())
    return crypto::ScopedRSA();
  rsa->e = BN_new();
  if (!rsa->e)
    return crypto::ScopedRSA();
  BN_set_word(rsa->e, kWellKnownExponent);
  rsa->n = BN_bin2bn(modulus_buffer, modulus_size, NULL);
  if (!rsa->n)
    return crypto::ScopedRSA();
  return rsa;
}

}  // namespace attestation
