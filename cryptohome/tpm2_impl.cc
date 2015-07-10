// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Tpm

#include "cryptohome/tpm2_impl.h"

#include <string>

#include <base/logging.h>
#include <trunks/error_codes.h>
#include <trunks/tpm_constants.h>
#include <trunks/trunks_factory.h>
#include <trunks/trunks_factory_impl.h>
#include <trunks/trunks_proxy.h>

using chromeos::SecureBlob;
using trunks::GetErrorString;
using trunks::TPM_RC;
using trunks::TPM_RC_SUCCESS;
using trunks::TrunksFactory;

namespace cryptohome {

Tpm2Impl::Tpm2Impl()
    : factory_(scoped_ptr<TrunksFactory>(new trunks::TrunksFactoryImpl())),
      state_(factory_->GetTpmState()),
      utility_(factory_->GetTpmUtility()) {}

Tpm2Impl::Tpm2Impl(TrunksFactory* factory)
    : factory_(scoped_ptr<TrunksFactory>(factory)),
      state_(factory_->GetTpmState()),
      utility_(factory_->GetTpmUtility()) {}

Tpm2Impl::~Tpm2Impl() {}

Tpm::TpmRetryAction Tpm2Impl::EncryptBlob(TpmKeyHandle key_handle,
                                          const SecureBlob& plaintext,
                                          const SecureBlob& key,
                                          SecureBlob* ciphertext) {
  LOG(FATAL) << "Not Implemented.";
  return Tpm::kTpmRetryFatal;
}

Tpm::TpmRetryAction Tpm2Impl::DecryptBlob(TpmKeyHandle key_handle,
                                          const SecureBlob& ciphertext,
                                          const SecureBlob& key,
                                          SecureBlob* plaintext) {
  LOG(FATAL) << "Not Implemented.";
  return Tpm::kTpmRetryFatal;
}

Tpm::TpmRetryAction Tpm2Impl::GetPublicKeyHash(TpmKeyHandle key_handle,
                                               SecureBlob* hash) {
  LOG(FATAL) << "Not Implemented.";
  return Tpm::kTpmRetryFatal;
}

bool Tpm2Impl::GetOwnerPassword(chromeos::Blob* owner_password) {
  if (!owner_password_.empty()) {
    owner_password->assign(owner_password_.begin(), owner_password_.end());
  } else {
    owner_password->clear();
  }
  return true;
}

void Tpm2Impl::SetOwnerPassword(const SecureBlob& owner_password) {
  SecureBlob tmp(owner_password);
  owner_password_.swap(tmp);
}

bool Tpm2Impl::PerformEnabledOwnedCheck(bool* enabled, bool* owned) {
  TPM_RC result = state_->Initialize();
  if (result != TPM_RC_SUCCESS) {
    if (enabled) {
      *enabled = false;
    }
    if (owned) {
      *owned = false;
    }
    LOG(ERROR) << "Error getting tpm status for owned check: "
               << GetErrorString(result);
    return false;
  }
  if (enabled) {
    *enabled = state_->IsEnabled();
  }
  if (owned) {
    *owned = state_->IsOwned();
  }
  return true;
}

bool Tpm2Impl::GetRandomData(size_t length, chromeos::Blob* data) {
  CHECK(data);
  std::string random_data;
  TPM_RC result = utility_->GenerateRandom(length,
                                           nullptr,  // No authorization.
                                           &random_data);
  if ((result != TPM_RC_SUCCESS) || (random_data.size() != length)) {
    LOG(ERROR) << "Error getting random data: " << GetErrorString(result);
    return false;
  }
  data->assign(random_data.begin(), random_data.end());
  return true;
}

bool Tpm2Impl::DefineLockOnceNvram(uint32_t index, size_t length) {
  if (owner_password_.empty()) {
    LOG(ERROR) << "Defining NVram needs owner_password.";
    return false;
  }
  scoped_ptr<trunks::HmacSession> session = factory_->GetHmacSession();
  TPM_RC result = session->StartUnboundSession(true  /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue(owner_password_.to_string());
  result = utility_->DefineNVSpace(index, length, session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error defining NVram space: " << GetErrorString(result);
    return false;
  }
  return true;
}

bool Tpm2Impl::DestroyNvram(uint32_t index) {
  if (owner_password_.empty()) {
    LOG(ERROR) << "Destroying NVram needs owner_password.";
    return false;
  }
  scoped_ptr<trunks::HmacSession> session = factory_->GetHmacSession();
  TPM_RC result = session->StartUnboundSession(true  /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue(owner_password_.to_string());
  result = utility_->DestroyNVSpace(index, session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error destroying NVram space: " << GetErrorString(result);
    return false;
  }
  return true;
}

bool Tpm2Impl::WriteNvram(uint32_t index, const SecureBlob& blob) {
  if (owner_password_.empty()) {
    LOG(ERROR) << "Writing NVram needs owner_password.";
    return false;
  }
  scoped_ptr<trunks::HmacSession> session = factory_->GetHmacSession();
  TPM_RC result = session->StartUnboundSession(true  /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue(owner_password_.to_string());
  result = utility_->WriteNVSpace(index,
                                  0,  // offset
                                  blob.to_string(),
                                  session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error writing to NVSpace: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue("");
  result = utility_->LockNVSpace(index, session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error locking NVSpace: " << GetErrorString(result);
    return false;
  }
  return true;
}

bool Tpm2Impl::ReadNvram(uint32_t index, SecureBlob* blob) {
  scoped_ptr<trunks::HmacSession> session = factory_->GetHmacSession();
  TPM_RC result = session->StartUnboundSession(true  /* Enable encryption */);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error starting a session: " << GetErrorString(result);
    return false;
  }
  session->SetEntityAuthorizationValue("");
  std::string nvram_data;
  result = utility_->ReadNVSpace(index,
                                 0,  // offset
                                 GetNvramSize(index),
                                 &nvram_data,
                                 session->GetDelegate());
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error reading from nvram: " << GetErrorString(result);
    return false;
  }
  blob->assign(nvram_data.begin(), nvram_data.end());
  return true;
}

bool Tpm2Impl::IsNvramDefined(uint32_t index) {
  trunks::TPMS_NV_PUBLIC nvram_public;
  TPM_RC result = utility_->GetNVSpacePublicArea(index, &nvram_public);
  if (trunks::GetFormatOneError(result) == trunks::TPM_RC_HANDLE) {
    return false;
  } else if (result == TPM_RC_SUCCESS) {
    return true;
  } else {
    LOG(ERROR) << "Error reading NV space for index " << index
               << " with error: " << GetErrorString(result);
  }
  return false;
}

bool Tpm2Impl::IsNvramLocked(uint32_t index) {
  trunks::TPMS_NV_PUBLIC nvram_public;
  TPM_RC result = utility_->GetNVSpacePublicArea(index, &nvram_public);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error reading NV space for index " << index
               << " with error: " << GetErrorString(result);
    return false;
  }
  return (nvram_public.attributes & trunks::TPMA_NV_WRITELOCKED) != 0;
}

unsigned int Tpm2Impl::GetNvramSize(uint32_t index) {
  trunks::TPMS_NV_PUBLIC nvram_public;
  TPM_RC result = utility_->GetNVSpacePublicArea(index, &nvram_public);
  if (result != TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error reading NV space for index " << index
               << " with error: " << GetErrorString(result);
    return 0;
  }
  return nvram_public.data_size;
}

bool Tpm2Impl::GetEndorsementPublicKey(SecureBlob* ek_public_key) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::GetEndorsementCredential(SecureBlob* credential) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::MakeIdentity(SecureBlob* identity_public_key_der,
                            SecureBlob* identity_public_key,
                            SecureBlob* identity_key_blob,
                            SecureBlob* identity_binding,
                            SecureBlob* identity_label,
                            SecureBlob* pca_public_key,
                            SecureBlob* endorsement_credential,
                            SecureBlob* platform_credential,
                            SecureBlob* conformance_credential) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::QuotePCR(int pcr_index,
                        const SecureBlob& identity_key_blob,
                        const SecureBlob& external_data,
                        SecureBlob* pcr_value,
                        SecureBlob* quoted_data,
                        SecureBlob* quote) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::SealToPCR0(const chromeos::Blob& value,
                          chromeos::Blob* sealed_value) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::Unseal(const chromeos::Blob& sealed_value,
                      chromeos::Blob* value) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::CreateCertifiedKey(const SecureBlob& identity_key_blob,
                                  const SecureBlob& external_data,
                                  SecureBlob* certified_public_key,
                                  SecureBlob* certified_public_key_der,
                                  SecureBlob* certified_key_blob,
                                  SecureBlob* certified_key_info,
                                  SecureBlob* certified_key_proof) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::CreateDelegate(const SecureBlob& identity_key_blob,
                              SecureBlob* delegate_blob,
                              SecureBlob* delegate_secret) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::ActivateIdentity(const SecureBlob& delegate_blob,
                                const SecureBlob& delegate_secret,
                                const SecureBlob& identity_key_blob,
                                const SecureBlob& encrypted_asym_ca,
                                const SecureBlob& encrypted_sym_ca,
                                SecureBlob* identity_credential) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::Sign(const SecureBlob& key_blob,
                    const SecureBlob& input,
                    int bound_pcr_index,
                    SecureBlob* signature) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::CreatePCRBoundKey(int pcr_index,
                                 const SecureBlob& pcr_value,
                                 SecureBlob* key_blob,
                                 SecureBlob* public_key_der) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::VerifyPCRBoundKey(int pcr_index,
                                 const SecureBlob& pcr_value,
                                 const SecureBlob& key_blob) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::ExtendPCR(int pcr_index, const SecureBlob& extension) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::ReadPCR(int pcr_index, SecureBlob* pcr_value) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::IsEndorsementKeyAvailable() {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::CreateEndorsementKey() {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::TakeOwnership(int max_timeout_tries,
                             const SecureBlob& owner_password) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::InitializeSrk(const SecureBlob& owner_password) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::ChangeOwnerPassword(const SecureBlob& previous_owner_password,
                                   const SecureBlob& owner_password) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::TestTpmAuth(const SecureBlob& owner_password) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::IsTransient(TpmRetryAction retry_action) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::WrapRsaKey(SecureBlob public_modulus,
                          SecureBlob prime_factor,
                          SecureBlob* wrapped_key) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

Tpm::TpmRetryAction Tpm2Impl::LoadWrappedKey(const SecureBlob& wrapped_key,
                                             ScopedKeyHandle* key_handle) {
  LOG(FATAL) << "Not Implemented.";
  return Tpm::kTpmRetryFatal;
}

bool Tpm2Impl::LegacyLoadCryptohomeKey(ScopedKeyHandle* key_handle,
                                       SecureBlob* key_blob) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

void Tpm2Impl::CloseHandle(TpmKeyHandle key_handle) {
  LOG(FATAL) << "Not Implemented.";
}

void Tpm2Impl::GetStatus(TpmKeyHandle key, TpmStatusInfo* status) {
  LOG(FATAL) << "Not Implemented.";
}

bool Tpm2Impl::GetDictionaryAttackInfo(int* counter,
                                       int* threshold,
                                       bool* lockout,
                                       int* seconds_remaining) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::ResetDictionaryAttackMitigation(
      const SecureBlob& delegate_blob,
      const SecureBlob& delegate_secret) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

}  // namespace cryptohome
