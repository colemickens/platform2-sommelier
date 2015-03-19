// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_STUB_TPM_H_
#define CRYPTOHOME_STUB_TPM_H_

#include <stdio.h>
#include <unistd.h>

#include "cryptohome/tpm.h"

using chromeos::SecureBlob;

namespace cryptohome {

class StubTpm : public Tpm {
 public:
  StubTpm() { }
  ~StubTpm() override { }

  // See tpm.h for comments
  bool EncryptBlob(TSS_HCONTEXT context_handle,
                   TSS_HKEY key_handle,
                   const SecureBlob& plaintext,
                   const SecureBlob& key,
                   SecureBlob* ciphertext,
                   TSS_RESULT* result) override { return false; }
  bool DecryptBlob(TSS_HCONTEXT context_handle,
                   TSS_HKEY key_handle,
                   const SecureBlob& ciphertext,
                   const SecureBlob& key,
                   SecureBlob* plaintext,
                   TSS_RESULT* result) override { return false; }
  TpmRetryAction GetPublicKeyHash(TSS_HCONTEXT context_handle,
                                  TSS_HKEY key_handle,
                                  SecureBlob* hash) override
    { return Tpm::kTpmRetryNone; }
  bool IsEnabled() const override { return false; }
  void SetIsEnabled(bool enabled) override {}
  bool IsOwned() const override { return false; }
  void SetIsOwned(bool owned) override {}
  bool ReadNvram(uint32_t index, SecureBlob* blob) override { return false; }
  bool IsNvramDefined(uint32_t index) override { return false; }
  bool IsNvramLocked(uint32_t index) override { return false; }
  unsigned int GetNvramSize(uint32_t index) override { return 0; }
  bool GetOwnerPassword(chromeos::Blob* owner_password) override
    { return false; }
  bool PerformEnabledOwnedCheck(bool* enabled, bool* owned) override
    { return false; }
  bool IsInitialized() const override { return false; }
  void SetIsInitialized(bool done) override {}
  bool IsBeingOwned() const override { return false; }
  void SetIsBeingOwned(bool value) override {}
  bool GetRandomData(size_t length, chromeos::Blob* data) override
    { return false; }
  bool DefineLockOnceNvram(uint32_t index, size_t length) override
    { return false; }
  bool DestroyNvram(uint32_t index) override { return false; }
  bool WriteNvram(uint32_t index, const SecureBlob& blob) override
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
  bool SealToPCR0(const chromeos::Blob& value,
                  chromeos::Blob* sealed_value) override { return false; }
  bool Unseal(const chromeos::Blob& sealed_value,
              chromeos::Blob* value) override { return false; }
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
  bool TssCompatibleEncrypt(const SecureBlob& key,
                            const SecureBlob& input,
                            SecureBlob* output) override { return false; }
  bool Sign(const SecureBlob& key_blob,
            const SecureBlob& der_encoded_input,
            SecureBlob* signature) override { return false; }
  bool CreatePCRBoundKey(int pcr_index,
                         const SecureBlob& pcr_value,
                         SecureBlob* key_blob,
                         SecureBlob* public_key_der) override { return false; }
  bool VerifyPCRBoundKey(int pcr_index,
                         const SecureBlob& pcr_value,
                         const SecureBlob& key_blob) override
    { return false; }
  bool ExtendPCR(int pcr_index, const SecureBlob& extension) override
    { return false; }
  bool ReadPCR(int pcr_index, SecureBlob* pcr_value) override { return false; }
  TSS_HCONTEXT ConnectContext() override { return 0; }
  void CloseContext(TSS_HCONTEXT context_handle) const override {}
  bool IsEndorsementKeyAvailable(TSS_HCONTEXT context_handle) override
    { return false; }
  bool CreateEndorsementKey(TSS_HCONTEXT context_handle) override
    { return false; }
  bool TakeOwnership(TSS_HCONTEXT context_handle,
                     int max_timeout_tries,
                     const SecureBlob& owner_password) override
    { return false; }
  bool ZeroSrkPassword(TSS_HCONTEXT context_handle,
                       const SecureBlob& owner_password) override
    { return false; }
  bool UnrestrictSrk(TSS_HCONTEXT context_handle,
                     const SecureBlob& owner_password) override
    { return false; }
  bool ChangeOwnerPassword(TSS_HCONTEXT context_handle,
                           const SecureBlob& previous_owner_password,
                           const SecureBlob& owner_password) override
    { return false; }
  bool GetTpmWithAuth(TSS_HCONTEXT context_handle,
                      const SecureBlob& owner_password,
                      TSS_HTPM* tpm_handle) override { return false; }
  bool TestTpmAuth(TSS_HTPM tpm_handle) override { return false; }
  void SetOwnerPassword(const SecureBlob& owner_password) override {}
  bool IsTransient(TSS_RESULT result) override { return false; }
  bool GetKeyBlob(TSS_HCONTEXT context_handle,
                  TSS_HKEY key_handle,
                  SecureBlob* data_out,
                  TSS_RESULT* result) const override { return false; }
  bool CreateWrappedRsaKey(TSS_HCONTEXT context_handle,
                           SecureBlob* wrapped_key) override { return false; }
  bool LoadWrappedKey(TSS_HCONTEXT context_handle,
                      const SecureBlob& wrapped_key,
                      TSS_HKEY* key_handle,
                      TSS_RESULT* result) const override { return false; }
  bool LoadKeyByUuid(TSS_HCONTEXT context_handle,
                     TSS_UUID key_uuid,
                     TSS_HKEY* key_handle,
                     SecureBlob* key_blob,
                     TSS_RESULT* result) const override { return false; }
  TpmRetryAction HandleError(TSS_RESULT result) override
    { return Tpm::kTpmRetryNone; }
  void GetStatus(
      TSS_HCONTEXT context, TSS_HKEY key, TpmStatusInfo* status) override {}
  bool GetDictionaryAttackInfo(int* counter,
                               int* threshold,
                               bool* lockout,
                               int* seconds_remaining) override
    { return false; }
  bool ResetDictionaryAttackMitigation(
      const SecureBlob& delegate_blob,
      const SecureBlob& delegate_secret) override { return false; }
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_STUB_TPM_H_
