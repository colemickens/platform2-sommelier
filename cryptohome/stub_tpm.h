// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_STUB_TPM_H_
#define CRYPTOHOME_STUB_TPM_H_

#include <stdio.h>
#include <unistd.h>

#include "cryptohome/tpm.h"

using brillo::SecureBlob;

namespace cryptohome {

class StubTpm : public Tpm {
 public:
  StubTpm() { }
  ~StubTpm() override { }

  // See tpm.h for comments
  TpmVersion GetVersion() override
    { return TpmVersion::TPM_UNKNOWN; }
  TpmRetryAction EncryptBlob(TpmKeyHandle key_handle,
                             const SecureBlob& plaintext,
                             const SecureBlob& key,
                             SecureBlob* ciphertext) override
    { return kTpmRetryFatal; }
  TpmRetryAction DecryptBlob(TpmKeyHandle key_handle,
                             const SecureBlob& ciphertext,
                             const SecureBlob& key,
                             SecureBlob* plaintext) override
    { return kTpmRetryFatal; }
  TpmRetryAction GetPublicKeyHash(TpmKeyHandle key_handle,
                                  SecureBlob* hash) override
    { return kTpmRetryNone; }
  bool IsEnabled() override { return false; }
  void SetIsEnabled(bool enabled) override {}
  bool IsOwned() override { return false; }
  void SetIsOwned(bool owned) override {}
  bool ReadNvram(uint32_t index, SecureBlob* blob) override { return false; }
  bool IsNvramDefined(uint32_t index) override { return false; }
  bool IsNvramLocked(uint32_t index) override { return false; }
  unsigned int GetNvramSize(uint32_t index) override { return 0; }
  bool GetOwnerPassword(brillo::Blob* owner_password) override
    { return false; }
  bool PerformEnabledOwnedCheck(bool* enabled, bool* owned) override
    { return false; }
  bool IsInitialized() override { return false; }
  void SetIsInitialized(bool done) override {}
  bool IsBeingOwned() override { return false; }
  void SetIsBeingOwned(bool value) override {}
  bool GetRandomData(size_t length, brillo::Blob* data) override
    { return false; }
  bool DefineNvram(uint32_t index, size_t length, uint32_t flags) override
    { return false; }
  bool DestroyNvram(uint32_t index) override { return false; }
  bool WriteNvram(uint32_t index, const SecureBlob& blob) override
    { return false; }
  bool WriteLockNvram(uint32_t index) override
    { return false; }
  bool GetEndorsementPublicKey(SecureBlob* ek_public_key) override
    { return false; }
  bool GetEndorsementCredential(SecureBlob* credential) override
    { return false; }
  bool MakeIdentity(SecureBlob* identity_public_key_der,
                    SecureBlob* identity_public_key,
                    SecureBlob* identity_key_blob,
                    SecureBlob* identity_binding,
                    SecureBlob* identity_label,
                    SecureBlob* pca_public_key,
                    SecureBlob* endorsement_credential,
                    SecureBlob* platform_credential,
                    SecureBlob* conformance_credential) override
    { return false; }
  bool QuotePCR(int pcr_index,
                const SecureBlob& identity_key_blob,
                const SecureBlob& external_data,
                SecureBlob* pcr_value,
                SecureBlob* quoted_data,
                SecureBlob* quote) override { return false; }
  bool SealToPCR0(const brillo::Blob& value,
                  brillo::Blob* sealed_value) override { return false; }
  bool Unseal(const brillo::Blob& sealed_value,
              brillo::Blob* value) override { return false; }
  bool CreateCertifiedKey(const SecureBlob& identity_key_blob,
                          const SecureBlob& external_data,
                          SecureBlob* certified_public_key,
                          SecureBlob* certified_public_key_der,
                          SecureBlob* certified_key_blob,
                          SecureBlob* certified_key_info,
                          SecureBlob* certified_key_proof) override
    { return false; }
  bool CreateDelegate(const SecureBlob& identity_key_blob,
                      SecureBlob* delegate_blob,
                      SecureBlob* delegate_secret) override
    { return false; }
  bool ActivateIdentity(const SecureBlob& delegate_blob,
                        const SecureBlob& delegate_secret,
                        const SecureBlob& identity_key_blob,
                        const SecureBlob& encrypted_asym_ca,
                        const SecureBlob& encrypted_sym_ca,
                        SecureBlob* identity_credential) override
    { return false; }
  bool Sign(const SecureBlob& key_blob,
            const SecureBlob& der_encoded_input,
            int bound_pcr_index,
            SecureBlob* signature) override { return false; }
  bool CreatePCRBoundKey(int pcr_index,
                         const SecureBlob& pcr_value,
                         SecureBlob* key_blob,
                         SecureBlob* public_key_der,
                         SecureBlob* creation_blob) override { return false; }
  bool VerifyPCRBoundKey(int pcr_index,
                         const SecureBlob& pcr_value,
                         const SecureBlob& key_blob,
                         const SecureBlob& creation_blob) override
    { return false; }
  bool ExtendPCR(int pcr_index, const SecureBlob& extension) override
    { return false; }
  bool ReadPCR(int pcr_index, SecureBlob* pcr_value) override { return false; }
  bool IsEndorsementKeyAvailable() override { return false; }
  bool CreateEndorsementKey() override { return false; }
  bool TakeOwnership(int max_timeout_tries,
                     const SecureBlob& owner_password) override
    { return false; }
  bool InitializeSrk(const SecureBlob& owner_password) override
    { return false; }
  bool ChangeOwnerPassword(const SecureBlob& previous_owner_password,
                           const SecureBlob& owner_password) override
    { return false; }
  bool TestTpmAuth(const SecureBlob& owner_password) override { return false; }
  void SetOwnerPassword(const SecureBlob& owner_password) override {}
  bool IsTransient(TpmRetryAction retry_action) override { return false; }
  bool WrapRsaKey(const SecureBlob& public_modulus,
                  const SecureBlob& prime_factor,
                  SecureBlob* wrapped_key) override { return false; }
  TpmRetryAction LoadWrappedKey(const SecureBlob& wrapped_key,
                                ScopedKeyHandle* key_handle) override
    { return kTpmRetryFatal; }
  bool LegacyLoadCryptohomeKey(ScopedKeyHandle* key_handle,
                               SecureBlob* key_blob) override { return false; }
  void CloseHandle(TpmKeyHandle key_handle) override {};
  void GetStatus(TpmKeyHandle key, TpmStatusInfo* status) override {}
  bool GetDictionaryAttackInfo(int* counter,
                               int* threshold,
                               bool* lockout,
                               int* seconds_remaining) override
    { return false; }
  bool ResetDictionaryAttackMitigation(
      const SecureBlob& delegate_blob,
      const SecureBlob& delegate_secret) override { return false; }
  void DeclareTpmFirmwareStable() override {}
  bool RemoveOwnerDependency(TpmOwnerDependency dependency) override
    { return true; }
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_STUB_TPM_H_
