// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_TPM2_IMPL_H_
#define CRYPTOHOME_TPM2_IMPL_H_

#include "cryptohome/tpm.h"

#include <trunks/hmac_session.h>
#include <trunks/tpm_state.h>
#include <trunks/tpm_utility.h>
#include <trunks/trunks_factory.h>

namespace cryptohome {

class Tpm2Impl : public Tpm {
 public:
  Tpm2Impl();
  // Takes ownership of factory.
  explicit Tpm2Impl(trunks::TrunksFactory* factory);
  virtual ~Tpm2Impl();
  // Tpm methods
  TpmRetryAction EncryptBlob(TpmKeyHandle key_handle,
                             const chromeos::SecureBlob& plaintext,
                             const chromeos::SecureBlob& key,
                             chromeos::SecureBlob* ciphertext) override;
  TpmRetryAction DecryptBlob(TpmKeyHandle key_handle,
                             const chromeos::SecureBlob& ciphertext,
                             const chromeos::SecureBlob& key,
                             chromeos::SecureBlob* plaintext) override;
  TpmRetryAction GetPublicKeyHash(TpmKeyHandle key_handle,
                                  chromeos::SecureBlob* hash) override;
  bool GetOwnerPassword(chromeos::Blob* owner_password) override;
  bool IsEnabled() const override { return !is_disabled_; }
  void SetIsEnabled(bool enabled) override { is_disabled_ = !enabled; }
  bool IsOwned() const override { return is_owned_; }
  void SetIsOwned(bool owned) override { is_owned_ = owned; }
  bool PerformEnabledOwnedCheck(bool* enabled, bool* owned) override;
  bool IsInitialized() const override { return initialized_; }
  void SetIsInitialized(bool done) override { initialized_ = done; }
  bool IsBeingOwned() const override { return is_being_owned_; }
  void SetIsBeingOwned(bool value) override { is_being_owned_ = value; }
  bool GetRandomData(size_t length, chromeos::Blob* data) override;
  bool DefineLockOnceNvram(uint32_t index, size_t length) override;
  bool DestroyNvram(uint32_t index) override;
  bool WriteNvram(uint32_t index, const chromeos::SecureBlob& blob) override;
  bool ReadNvram(uint32_t index, chromeos::SecureBlob* blob) override;
  bool IsNvramDefined(uint32_t index) override;
  bool IsNvramLocked(uint32_t index) override;
  unsigned int GetNvramSize(uint32_t index) override;
  bool GetEndorsementPublicKey(chromeos::SecureBlob* ek_public_key) override;
  bool GetEndorsementCredential(chromeos::SecureBlob* credential) override;
  bool MakeIdentity(chromeos::SecureBlob* identity_public_key_der,
                    chromeos::SecureBlob* identity_public_key,
                    chromeos::SecureBlob* identity_key_blob,
                    chromeos::SecureBlob* identity_binding,
                    chromeos::SecureBlob* identity_label,
                    chromeos::SecureBlob* pca_public_key,
                    chromeos::SecureBlob* endorsement_credential,
                    chromeos::SecureBlob* platform_credential,
                    chromeos::SecureBlob* conformance_credential) override;
  bool QuotePCR(int pcr_index,
                const chromeos::SecureBlob& identity_key_blob,
                const chromeos::SecureBlob& external_data,
                chromeos::SecureBlob* pcr_value,
                chromeos::SecureBlob* quoted_data,
                chromeos::SecureBlob* quote) override;
  bool SealToPCR0(const chromeos::Blob& value,
                  chromeos::Blob* sealed_value) override;
  bool Unseal(const chromeos::Blob& sealed_value,
              chromeos::Blob* value) override;
  bool CreateCertifiedKey(
      const chromeos::SecureBlob& identity_key_blob,
      const chromeos::SecureBlob& external_data,
      chromeos::SecureBlob* certified_public_key,
      chromeos::SecureBlob* certified_public_key_der,
      chromeos::SecureBlob* certified_key_blob,
      chromeos::SecureBlob* certified_key_info,
      chromeos::SecureBlob* certified_key_proof) override;
  bool CreateDelegate(const chromeos::SecureBlob& identity_key_blob,
                      chromeos::SecureBlob* delegate_blob,
                      chromeos::SecureBlob* delegate_secret) override;
  bool ActivateIdentity(const chromeos::SecureBlob& delegate_blob,
                        const chromeos::SecureBlob& delegate_secret,
                        const chromeos::SecureBlob& identity_key_blob,
                        const chromeos::SecureBlob& encrypted_asym_ca,
                        const chromeos::SecureBlob& encrypted_sym_ca,
                        chromeos::SecureBlob* identity_credential) override;
  bool Sign(const chromeos::SecureBlob& key_blob,
            const chromeos::SecureBlob& input,
            int bound_pcr_index,
            chromeos::SecureBlob* signature) override;
  bool CreatePCRBoundKey(int pcr_index,
                         const chromeos::SecureBlob& pcr_value,
                         chromeos::SecureBlob* key_blob,
                         chromeos::SecureBlob* public_key_der,
                         chromeos::SecureBlob* creation_blob) override;
  bool VerifyPCRBoundKey(int pcr_index,
                         const chromeos::SecureBlob& pcr_value,
                         const chromeos::SecureBlob& key_blob,
                         const chromeos::SecureBlob& creation_blob) override;
  bool ExtendPCR(int pcr_index, const chromeos::SecureBlob& extension) override;
  bool ReadPCR(int pcr_index, chromeos::SecureBlob* pcr_value) override;
  bool IsEndorsementKeyAvailable() override;
  bool CreateEndorsementKey() override;
  bool TakeOwnership(int max_timeout_tries,
                     const chromeos::SecureBlob& owner_password) override;
  bool InitializeSrk(const chromeos::SecureBlob& owner_password) override;
  bool ChangeOwnerPassword(const chromeos::SecureBlob& previous_owner_password,
                           const chromeos::SecureBlob& owner_password) override;
  bool TestTpmAuth(const chromeos::SecureBlob& owner_password) override;
  void SetOwnerPassword(const chromeos::SecureBlob& owner_password) override;
  bool IsTransient(TpmRetryAction retry_action) override;
  bool WrapRsaKey(SecureBlob public_modulus,
                  SecureBlob prime_factor,
                  SecureBlob* wrapped_key) override;
  TpmRetryAction LoadWrappedKey(const chromeos::SecureBlob& wrapped_key,
                                ScopedKeyHandle* key_handle) override;
  bool LegacyLoadCryptohomeKey(ScopedKeyHandle* key_handle,
                               chromeos::SecureBlob* key_blob) override;
  void CloseHandle(TpmKeyHandle key_handle) override;
  void GetStatus(TpmKeyHandle key, TpmStatusInfo* status) override;
  bool GetDictionaryAttackInfo(int* counter,
                               int* threshold,
                               bool* lockout,
                               int* seconds_remaining) override;
  bool ResetDictionaryAttackMitigation(
      const chromeos::SecureBlob& delegate_blob,
      const chromeos::SecureBlob& delegate_secret) override;

 private:
  scoped_ptr<trunks::TrunksFactory> factory_;
  scoped_ptr<trunks::TpmState> state_;
  scoped_ptr<trunks::TpmUtility> utility_;

  bool is_disabled_ = false;
  bool is_owned_ = false;
  bool initialized_ = false;
  bool is_being_owned_ = false;

  chromeos::SecureBlob owner_password_;

  DISALLOW_COPY_AND_ASSIGN(Tpm2Impl);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM2_IMPL_H_
