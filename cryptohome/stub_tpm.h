// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_STUB_TPM_H_
#define CRYPTOHOME_STUB_TPM_H_

#include <stdio.h>
#include <unistd.h>

#include "cryptohome/tpm.h"

namespace cryptohome {

class StubTpm : public Tpm {
 public:
  StubTpm() { }
  virtual ~StubTpm() { }

  // See tpm.h for comments
  virtual bool EncryptBlob(TSS_HCONTEXT context_handle,
                           TSS_HKEY key_handle,
                           const chromeos::SecureBlob& plaintext,
                           const chromeos::SecureBlob& key,
                           chromeos::SecureBlob* ciphertext,
                           TSS_RESULT* result) { return false; }
  virtual bool DecryptBlob(TSS_HCONTEXT context_handle,
                           TSS_HKEY key_handle,
                           const chromeos::SecureBlob& ciphertext,
                           const chromeos::SecureBlob& key,
                           chromeos::SecureBlob* plaintext,
                           TSS_RESULT* result) { return false; }
  virtual TpmRetryAction GetPublicKeyHash(
      TSS_HCONTEXT context_handle,
      TSS_HKEY key_handle,
      chromeos::SecureBlob* hash) { return Tpm::kTpmRetryNone; }
  virtual bool Connect(TpmRetryAction* retry_action) { return false; }
  virtual bool IsConnected() { return false; }
  virtual bool IsEnabled() const { return false; }
  virtual void SetIsEnabled(bool enabled) {}
  virtual bool IsOwned() const { return false; }
  virtual void SetIsOwned(bool owned) {}
  virtual bool ReadNvram(uint32_t index,
                         chromeos::SecureBlob* blob) { return false; }
  virtual bool IsNvramDefined(uint32_t index) { return false; }
  virtual bool IsNvramLocked(uint32_t index) { return false; }
  virtual unsigned int GetNvramSize(uint32_t index) { return 0; }
  virtual void Disconnect() { }
  virtual bool GetOwnerPassword(
    chromeos::Blob* owner_password) { return false; }
  virtual bool Encrypt(const chromeos::SecureBlob& plaintext,
                       const chromeos::SecureBlob& key,
                       chromeos::SecureBlob* ciphertext,
                       TpmRetryAction* retry_action) { return false; }
  virtual bool Decrypt(const chromeos::SecureBlob& ciphertext,
                       const chromeos::SecureBlob& key,
                       chromeos::SecureBlob* plaintext,
                       TpmRetryAction* retry_action) { return false; }
  virtual bool PerformEnabledOwnedCheck(bool* enabled,
                                        bool* owned) { return false; }
  virtual bool IsInitialized() const { return false; }
  virtual void SetIsInitialized(bool done) {}
  virtual bool IsBeingOwned() const { return false; }
  virtual void SetIsBeingOwned(bool value) {}
  virtual bool GetRandomData(size_t length,
                             chromeos::Blob* data) { return false; }
  virtual bool DefineLockOnceNvram(uint32_t index,
                                   size_t length) { return false; }
  virtual bool DestroyNvram(uint32_t index) { return false; }
  virtual bool WriteNvram(uint32_t index,
                          const chromeos::SecureBlob& blob) { return false; }
  virtual bool GetEndorsementPublicKey(chromeos::SecureBlob* ek_public_key)
    { return false; }
  virtual bool GetEndorsementCredential(chromeos::SecureBlob* credential)
    { return false; }
  virtual bool MakeIdentity(chromeos::SecureBlob* identity_public_key_der,
                            chromeos::SecureBlob* identity_public_key,
                            chromeos::SecureBlob* identity_key_blob,
                            chromeos::SecureBlob* identity_binding,
                            chromeos::SecureBlob* identity_label,
                            chromeos::SecureBlob* pca_public_key,
                            chromeos::SecureBlob* endorsement_credential,
                            chromeos::SecureBlob* platform_credential,
                            chromeos::SecureBlob* conformance_credential)
    { return false; }
  virtual bool QuotePCR(int pcr_index,
                        const chromeos::SecureBlob& identity_key_blob,
                        const chromeos::SecureBlob& external_data,
                        chromeos::SecureBlob* pcr_value,
                        chromeos::SecureBlob* quoted_data,
                        chromeos::SecureBlob* quote) { return false; }
  virtual bool SealToPCR0(const chromeos::Blob& value,
                          chromeos::Blob* sealed_value) { return false; }
  virtual bool Unseal(const chromeos::Blob& sealed_value,
                      chromeos::Blob* value) { return false; }
  virtual bool CreateCertifiedKey(
      const chromeos::SecureBlob& identity_key_blob,
      const chromeos::SecureBlob& external_data,
      chromeos::SecureBlob* certified_public_key,
      chromeos::SecureBlob* certified_public_key_der,
      chromeos::SecureBlob* certified_key_blob,
      chromeos::SecureBlob* certified_key_info,
      chromeos::SecureBlob* certified_key_proof) { return false; }
  virtual bool CreateDelegate(const chromeos::SecureBlob& identity_key_blob,
                              chromeos::SecureBlob* delegate_blob,
                              chromeos::SecureBlob* delegate_secret)
    { return false; }
  virtual bool ActivateIdentity(const chromeos::SecureBlob& delegate_blob,
                                const chromeos::SecureBlob& delegate_secret,
                                const chromeos::SecureBlob& identity_key_blob,
                                const chromeos::SecureBlob& encrypted_asym_ca,
                                const chromeos::SecureBlob& encrypted_sym_ca,
                                chromeos::SecureBlob* identity_credential)
    { return false; }
  virtual bool TssCompatibleEncrypt(const chromeos::SecureBlob& key,
                                    const chromeos::SecureBlob& input,
                                    chromeos::SecureBlob* output)
    { return false; }
  virtual bool TpmCompatibleOAEPEncrypt(RSA* key,
                                        const chromeos::SecureBlob& input,
                                        chromeos::SecureBlob* output)
    { return false; }
  virtual bool Sign(const chromeos::SecureBlob& key_blob,
                    const chromeos::SecureBlob& der_encoded_input,
                    chromeos::SecureBlob* signature) { return false; }
  virtual bool CreatePCRBoundKey(
      int pcr_index,
      const chromeos::SecureBlob& pcr_value,
      chromeos::SecureBlob* key_blob,
      chromeos::SecureBlob* public_key_der) { return false; }
  virtual bool VerifyPCRBoundKey(
      int pcr_index,
      const chromeos::SecureBlob& pcr_value,
      const chromeos::SecureBlob& key_blob) { return false; }
  virtual bool ExtendPCR(int pcr_index, const chromeos::SecureBlob& extension)
    { return false; }
  virtual bool ReadPCR(int pcr_index, chromeos::SecureBlob* pcr_value)
    { return false; }
  virtual bool OpenAndConnectTpm(TSS_HCONTEXT* context_handle,
                                 TSS_RESULT* result) { return false; }
  virtual TSS_HCONTEXT ConnectContext() { return 0; }
  virtual void CloseContext(TSS_HCONTEXT context_handle) const {}
  virtual bool IsEndorsementKeyAvailable(TSS_HCONTEXT context_handle)
    { return false; }
  virtual bool CreateEndorsementKey(TSS_HCONTEXT context_handle)
    { return false; }
  virtual bool TakeOwnership(TSS_HCONTEXT context_handle, int max_timeout_tries,
                             const chromeos::SecureBlob& owner_password)
    { return false; }
  virtual bool ZeroSrkPassword(TSS_HCONTEXT context_handle,
                               const chromeos::SecureBlob& owner_password)
    { return false; }
  virtual bool UnrestrictSrk(TSS_HCONTEXT context_handle,
                             const chromeos::SecureBlob& owner_password)
    { return false; }
  virtual bool ChangeOwnerPassword(
      TSS_HCONTEXT context_handle,
      const chromeos::SecureBlob& previous_owner_password,
      const chromeos::SecureBlob& owner_password) { return false; }
  virtual bool GetTpmWithAuth(TSS_HCONTEXT context_handle,
                              const chromeos::SecureBlob& owner_password,
                              TSS_HTPM* tpm_handle) { return false; }
  virtual bool TestTpmAuth(TSS_HTPM tpm_handle) { return false; }
  virtual void SetOwnerPassword(const chromeos::SecureBlob& owner_password) {}
  virtual bool LoadSrk(TSS_HCONTEXT context_handle, TSS_HKEY* srk_handle,
                       TSS_RESULT* result) const { return false; }
  virtual bool IsTransient(TSS_RESULT result) { return false; }
  virtual bool GetPublicKeyBlob(TSS_HCONTEXT context_handle,
                                TSS_HKEY key_handle,
                                chromeos::SecureBlob* data_out,
                                TSS_RESULT* result) const { return false; }
  virtual bool GetKeyBlob(TSS_HCONTEXT context_handle,
                          TSS_HKEY key_handle,
                          chromeos::SecureBlob* data_out,
                          TSS_RESULT* result) const { return false; }
  virtual bool CreateWrappedRsaKey(TSS_HCONTEXT context_handle,
                                   chromeos::SecureBlob* wrapped_key)
    { return false; }
  virtual bool LoadWrappedKey(TSS_HCONTEXT context_handle,
                              const chromeos::SecureBlob& wrapped_key,
                              TSS_HKEY* key_handle,
                              TSS_RESULT* result) const { return false; }
  virtual bool LoadKeyByUuid(TSS_HCONTEXT context_handle,
                             TSS_UUID key_uuid,
                             TSS_HKEY* key_handle,
                             chromeos::SecureBlob* key_blob,
                             TSS_RESULT* result) const { return false; }
  virtual TpmRetryAction HandleError(TSS_RESULT result)
    { return Tpm::kTpmRetryNone; }
  virtual void GetStatus(TSS_HCONTEXT context,
                         TSS_HKEY key,
                         TpmStatusInfo* status) {}
  virtual bool GetDictionaryAttackInfo(int* counter,
                                       int* threshold,
                                       bool* lockout,
                                       int* seconds_remaining) { return false; }
  virtual bool ResetDictionaryAttackMitigation(
      const chromeos::SecureBlob& delegate_blob,
      const chromeos::SecureBlob& delegate_secret) { return false; }
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_STUB_TPM_H_
