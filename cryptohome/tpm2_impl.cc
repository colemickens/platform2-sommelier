// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Tpm

#include "cryptohome/tpm2_impl.h"

#include <base/logging.h>

using chromeos::SecureBlob;

namespace cryptohome {

Tpm2Impl::Tpm2Impl() {}

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
  LOG(FATAL) << "Not Implemented.";
  return false;
}

void Tpm2Impl::SetOwnerPassword(const SecureBlob& owner_password) {
  LOG(FATAL) << "Not Implemented.";
}

bool Tpm2Impl::PerformEnabledOwnedCheck(bool* enabled, bool* owned) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::GetRandomData(size_t length, chromeos::Blob* data) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::DefineLockOnceNvram(uint32_t index, size_t length) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::DestroyNvram(uint32_t index) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::WriteNvram(uint32_t index, const SecureBlob& blob) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::ReadNvram(uint32_t index, SecureBlob* blob) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::IsNvramDefined(uint32_t index) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

bool Tpm2Impl::IsNvramLocked(uint32_t index) {
  LOG(FATAL) << "Not Implemented.";
  return false;
}

unsigned int Tpm2Impl::GetNvramSize(uint32_t index) {
  LOG(FATAL) << "Not Implemented.";
  return 0;
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
                    const SecureBlob& der_encoded_input,
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

bool Tpm2Impl::CreateWrappedRsaKey(SecureBlob* wrapped_key) {
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
