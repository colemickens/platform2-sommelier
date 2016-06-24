// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_TPM2_IMPL_H_
#define CRYPTOHOME_TPM2_IMPL_H_

#include <map>
#include <memory>

#include <base/threading/platform_thread.h>
#include <trunks/hmac_session.h>
#include <trunks/tpm_state.h>
#include <trunks/tpm_utility.h>
#include <trunks/trunks_factory.h>
#include <trunks/trunks_factory_impl.h>

#include "cryptohome/tpm.h"

namespace cryptohome {

const uint32_t kDefaultTpmRsaModulusSize = 2048;
const uint32_t kDefaultTpmPublicExponent = 0x10001;
const uint32_t kLockboxPCR = 15;

class Tpm2Impl : public Tpm {
 public:
  Tpm2Impl() = default;
  // Does not take ownership of |factory|.
  explicit Tpm2Impl(trunks::TrunksFactory* factory);
  virtual ~Tpm2Impl() = default;

  // Tpm methods
  TpmRetryAction EncryptBlob(TpmKeyHandle key_handle,
                             const brillo::SecureBlob& plaintext,
                             const brillo::SecureBlob& key,
                             brillo::SecureBlob* ciphertext) override;
  TpmRetryAction DecryptBlob(TpmKeyHandle key_handle,
                             const brillo::SecureBlob& ciphertext,
                             const brillo::SecureBlob& key,
                             brillo::SecureBlob* plaintext) override;
  TpmRetryAction GetPublicKeyHash(TpmKeyHandle key_handle,
                                  brillo::SecureBlob* hash) override;
  bool GetOwnerPassword(brillo::Blob* owner_password) override;
  bool IsEnabled() const override { return !is_disabled_; }
  void SetIsEnabled(bool enabled) override { is_disabled_ = !enabled; }
  bool IsOwned() const override { return is_owned_; }
  void SetIsOwned(bool owned) override { is_owned_ = owned; }
  bool PerformEnabledOwnedCheck(bool* enabled, bool* owned) override;
  bool IsInitialized() const override { return initialized_; }
  void SetIsInitialized(bool done) override { initialized_ = done; }
  bool IsBeingOwned() const override { return is_being_owned_; }
  void SetIsBeingOwned(bool value) override { is_being_owned_ = value; }
  bool GetRandomData(size_t length, brillo::Blob* data) override;
  bool DefineNvram(uint32_t index, size_t length, uint32_t flags) override;
  bool DestroyNvram(uint32_t index) override;
  bool WriteNvram(uint32_t index, const brillo::SecureBlob& blob) override;
  bool ReadNvram(uint32_t index, brillo::SecureBlob* blob) override;
  bool IsNvramDefined(uint32_t index) override;
  bool IsNvramLocked(uint32_t index) override;
  unsigned int GetNvramSize(uint32_t index) override;
  bool GetEndorsementPublicKey(brillo::SecureBlob* ek_public_key) override;
  bool GetEndorsementCredential(brillo::SecureBlob* credential) override;
  bool MakeIdentity(brillo::SecureBlob* identity_public_key_der,
                    brillo::SecureBlob* identity_public_key,
                    brillo::SecureBlob* identity_key_blob,
                    brillo::SecureBlob* identity_binding,
                    brillo::SecureBlob* identity_label,
                    brillo::SecureBlob* pca_public_key,
                    brillo::SecureBlob* endorsement_credential,
                    brillo::SecureBlob* platform_credential,
                    brillo::SecureBlob* conformance_credential) override;
  bool QuotePCR(int pcr_index,
                const brillo::SecureBlob& identity_key_blob,
                const brillo::SecureBlob& external_data,
                brillo::SecureBlob* pcr_value,
                brillo::SecureBlob* quoted_data,
                brillo::SecureBlob* quote) override;
  bool SealToPCR0(const brillo::Blob& value,
                  brillo::Blob* sealed_value) override;
  bool Unseal(const brillo::Blob& sealed_value, brillo::Blob* value) override;
  bool CreateCertifiedKey(const brillo::SecureBlob& identity_key_blob,
                          const brillo::SecureBlob& external_data,
                          brillo::SecureBlob* certified_public_key,
                          brillo::SecureBlob* certified_public_key_der,
                          brillo::SecureBlob* certified_key_blob,
                          brillo::SecureBlob* certified_key_info,
                          brillo::SecureBlob* certified_key_proof) override;
  bool CreateDelegate(const brillo::SecureBlob& identity_key_blob,
                      brillo::SecureBlob* delegate_blob,
                      brillo::SecureBlob* delegate_secret) override;
  bool ActivateIdentity(const brillo::SecureBlob& delegate_blob,
                        const brillo::SecureBlob& delegate_secret,
                        const brillo::SecureBlob& identity_key_blob,
                        const brillo::SecureBlob& encrypted_asym_ca,
                        const brillo::SecureBlob& encrypted_sym_ca,
                        brillo::SecureBlob* identity_credential) override;
  bool Sign(const brillo::SecureBlob& key_blob,
            const brillo::SecureBlob& input,
            int bound_pcr_index,
            brillo::SecureBlob* signature) override;
  bool CreatePCRBoundKey(int pcr_index,
                         const brillo::SecureBlob& pcr_value,
                         brillo::SecureBlob* key_blob,
                         brillo::SecureBlob* public_key_der,
                         brillo::SecureBlob* creation_blob) override;
  bool VerifyPCRBoundKey(int pcr_index,
                         const brillo::SecureBlob& pcr_value,
                         const brillo::SecureBlob& key_blob,
                         const brillo::SecureBlob& creation_blob) override;
  bool ExtendPCR(int pcr_index, const brillo::SecureBlob& extension) override;
  bool ReadPCR(int pcr_index, brillo::SecureBlob* pcr_value) override;
  bool IsEndorsementKeyAvailable() override;
  bool CreateEndorsementKey() override;
  bool TakeOwnership(int max_timeout_tries,
                     const brillo::SecureBlob& owner_password) override;
  bool InitializeSrk(const brillo::SecureBlob& owner_password) override;
  bool ChangeOwnerPassword(const brillo::SecureBlob& previous_owner_password,
                           const brillo::SecureBlob& owner_password) override;
  bool TestTpmAuth(const brillo::SecureBlob& owner_password) override;
  void SetOwnerPassword(const brillo::SecureBlob& owner_password) override;
  bool IsTransient(TpmRetryAction retry_action) override;
  bool WrapRsaKey(const brillo::SecureBlob& public_modulus,
                  const brillo::SecureBlob& prime_factor,
                  brillo::SecureBlob* wrapped_key) override;
  TpmRetryAction LoadWrappedKey(const brillo::SecureBlob& wrapped_key,
                                ScopedKeyHandle* key_handle) override;
  bool LegacyLoadCryptohomeKey(ScopedKeyHandle* key_handle,
                               brillo::SecureBlob* key_blob) override;
  void CloseHandle(TpmKeyHandle key_handle) override;
  void GetStatus(TpmKeyHandle key, TpmStatusInfo* status) override;
  bool GetDictionaryAttackInfo(int* counter,
                               int* threshold,
                               bool* lockout,
                               int* seconds_remaining) override;
  bool ResetDictionaryAttackMitigation(
      const brillo::SecureBlob& delegate_blob,
      const brillo::SecureBlob& delegate_secret) override;

 private:
  // This object may be used across multiple threads but the Trunks D-Bus proxy
  // can only be used on a single thread. This structure holds all Trunks client
  // objects for a particular thread.
  struct TrunksClientContext {
    trunks::TrunksFactory* factory;
    std::unique_ptr<trunks::TrunksFactoryImpl> factory_impl;
    std::unique_ptr<trunks::TpmState> tpm_state;
    std::unique_ptr<trunks::TpmUtility> tpm_utility;
  };

  // Gets the trunks objects for the current thread, initializing new ones if
  // necessary. Returns true on success.
  bool GetTrunksContext(TrunksClientContext** trunks);

  // This method given a Tpm generated public area, returns the DER encoded
  // public key.
  bool PublicAreaToPublicKeyDER(const trunks::TPMT_PUBLIC& public_area,
                                brillo::SecureBlob* public_key_der);

  std::map<base::PlatformThreadId, std::unique_ptr<TrunksClientContext>>
      trunks_contexts_;
  TrunksClientContext external_trunks_context_;
  bool has_external_trunks_context_ = false;

  // For now, be optimistic by default because tpm_managerd should be taking
  // care of this and until the integration is farther along we're blind to the
  // detailed status.
  // TODO(dkrahn): crbug.com/623100 - Finish tpm_managerd integration.
  bool is_disabled_ = false;
  bool is_owned_ = true;
  bool initialized_ = true;
  bool is_being_owned_ = false;

  brillo::SecureBlob owner_password_;

  DISALLOW_COPY_AND_ASSIGN(Tpm2Impl);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM2_IMPL_H_
