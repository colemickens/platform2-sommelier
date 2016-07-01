//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "attestation/common/tpm_utility_v2.h"

#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <brillo/bind_lambda.h>

namespace attestation {

TpmUtilityV2::TpmUtilityV2(tpm_manager::TpmOwnershipInterface* tpm_owner,
                           tpm_manager::TpmNvramInterface* tpm_nvram)
    : tpm_owner_(tpm_owner), tpm_nvram_(tpm_nvram) {}

bool TpmUtilityV2::Initialize() {
  if (!tpm_manager_thread_.StartWithOptions(base::Thread::Options(
          base::MessageLoopForIO::TYPE_IO, 0 /* Default stack size. */))) {
    LOG(ERROR) << "Failed to start tpm_manager thread.";
    return false;
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    base::WaitableEvent event(true /* manual_reset */,
                              false /* initially_signaled */);
    tpm_manager_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind([this, &event]() {
          default_tpm_owner_ =
              base::MakeUnique<tpm_manager::TpmOwnershipDBusProxy>();
          default_tpm_nvram_ =
              base::MakeUnique<tpm_manager::TpmNvramDBusProxy>();
          if (default_tpm_owner_->Initialize()) {
            tpm_owner_ = default_tpm_owner_.get();
          }
          if (default_tpm_nvram_->Initialize()) {
            tpm_nvram_ = default_tpm_nvram_.get();
          }
          event.Signal();
        }));
    event.Wait();
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    LOG(ERROR) << "Failed to initialize tpm_managerd clients.";
    return false;
  }
  return true;
}

bool TpmUtilityV2::IsTpmReady() {
  if (!is_ready_) {
    tpm_manager::GetTpmStatusReply tpm_status;
    SendTpmManagerRequestAndWait(
        base::Bind(&tpm_manager::TpmOwnershipInterface::GetTpmStatus,
                   base::Unretained(tpm_owner_),
                   tpm_manager::GetTpmStatusRequest()),
        &tpm_status);
    is_ready_ = (tpm_status.enabled() && tpm_status.owned() &&
                 !tpm_status.dictionary_attack_lockout_in_effect());
  }
  return is_ready_;
}

bool TpmUtilityV2::ActivateIdentity(const std::string& delegate_blob,
                                    const std::string& delegate_secret,
                                    const std::string& identity_key_blob,
                                    const std::string& asym_ca_contents,
                                    const std::string& sym_ca_attestation,
                                    std::string* credential) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::ActivateIdentityForTpm2(
    KeyType key_type,
    const std::string& identity_key_blob,
    const std::string& encrypted_seed,
    const std::string& credential_mac,
    const std::string& wrapped_credential,
    std::string* credential) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::CreateCertifiedKey(KeyType key_type,
                                      KeyUsage key_usage,
                                      const std::string& identity_key_blob,
                                      const std::string& external_data,
                                      std::string* key_blob,
                                      std::string* public_key,
                                      std::string* public_key_tpm_format,
                                      std::string* key_info,
                                      std::string* proof) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::SealToPCR0(const std::string& data,
                              std::string* sealed_data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Unseal(const std::string& sealed_data, std::string* data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::GetEndorsementPublicKey(KeyType key_type,
                                           std::string* public_key) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::GetEndorsementCertificate(KeyType key_type,
                                             std::string* certificate) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Unbind(const std::string& key_blob,
                          const std::string& bound_data,
                          std::string* data) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::Sign(const std::string& key_blob,
                        const std::string& data_to_sign,
                        std::string* signature) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::CreateRestrictedKey(KeyType key_type,
                                       KeyUsage key_usage,
                                       std::string* public_key,
                                       std::string* private_key_blob) {
  LOG(ERROR) << __func__ << ": Not Implemented.";
  return false;
}

bool TpmUtilityV2::QuotePCR(int pcr_index,
                            const std::string& key_blob,
                            std::string* quoted_pcr_value,
                            std::string* quoted_data,
                            std::string* quote) {
  LOG(ERROR) << __func__ << ": Not implemented.";
  return false;
}

template <typename ReplyProtoType, typename MethodType>
void TpmUtilityV2::SendTpmManagerRequestAndWait(const MethodType& method,
                                                ReplyProtoType* reply_proto) {
  base::WaitableEvent event(true /* manual_reset */,
                            false /* initially_signaled */);
  auto callback =
      base::Bind([reply_proto, &event](const ReplyProtoType& reply) {
        *reply_proto = reply;
        event.Signal();
      });
  tpm_manager_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind([this, &method, &callback]() { method.Run(callback); }));
  event.Wait();
}

}  // namespace attestation
