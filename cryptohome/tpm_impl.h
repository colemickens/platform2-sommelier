// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_TPM_IMPL_H_
#define CRYPTOHOME_TPM_IMPL_H_

#include "cryptohome/tpm.h"

#include <trousers/scoped_tss_type.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

namespace cryptohome {

// See README.lockbox for information on how this was selected.
const uint32_t kLockboxIndex = 0x20000004;

class TpmImpl : public Tpm {
 public:
  TpmImpl();
  virtual ~TpmImpl();
  // Tpm methods
  TpmVersion GetVersion() override { return TpmVersion::TPM_1_2; }
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
  bool IsEnabled() override { return !is_disabled_; }
  void SetIsEnabled(bool enabled) override { is_disabled_ = !enabled; }
  bool IsOwned() override { return is_owned_; }
  void SetIsOwned(bool owned) override { is_owned_ = owned; }
  bool PerformEnabledOwnedCheck(bool* enabled, bool* owned) override;
  bool IsInitialized() override { return initialized_; }
  void SetIsInitialized(bool done) override { initialized_ = done; }
  bool IsBeingOwned() override { return is_being_owned_; }
  void SetIsBeingOwned(bool value) override { is_being_owned_ = value; }
  bool GetRandomData(size_t length, brillo::Blob* data) override;
  bool DefineNvram(uint32_t index, size_t length, uint32_t flags) override;
  bool DestroyNvram(uint32_t index) override;
  bool WriteNvram(uint32_t index, const brillo::SecureBlob& blob) override;
  bool ReadNvram(uint32_t index, brillo::SecureBlob* blob) override;
  bool IsNvramDefined(uint32_t index) override;
  bool IsNvramLocked(uint32_t index) override;
  bool WriteLockNvram(uint32_t index) override;
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
  bool Unseal(const brillo::Blob& sealed_value,
              brillo::Blob* value) override;
  bool CreateCertifiedKey(
      const brillo::SecureBlob& identity_key_blob,
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
  void DeclareTpmFirmwareStable() override {}

 private:
  // Connects to the TPM and return its context at |context_handle|.
  bool OpenAndConnectTpm(TSS_HCONTEXT* context_handle,
                         TSS_RESULT* result);

  // Gets the Public Key blob associated with |key_handle|.
  bool GetPublicKeyBlob(TSS_HCONTEXT context_handle,
                        TSS_HKEY key_handle,
                        brillo::SecureBlob* data_out,
                        TSS_RESULT* result) const;

  // Gets the key blob associated with |key_handle|.
  bool GetKeyBlob(TSS_HCONTEXT context_handle,
                  TSS_HKEY key_handle,
                  brillo::SecureBlob* data_out,
                  TSS_RESULT* result) const;

  // Gets a handle to the SRK.
  bool LoadSrk(TSS_HCONTEXT context_handle, TSS_HKEY* srk_handle,
               TSS_RESULT* result) const;

  // Zeros the SRK password (sets it to an empty string)
  //
  // Parameters
  //   context_handle - The context handle for the TPM session
  //   owner_password - The owner password for the TPM
  bool ZeroSrkPassword(TSS_HCONTEXT context_handle,
                       const brillo::SecureBlob& owner_password);

  // Removes usage restrictions on the SRK
  //
  // Parameters
  //   context_handle - The context handle for the TPM session
  //   owner_password - The owner password for the TPM
  bool UnrestrictSrk(TSS_HCONTEXT context_handle,
                     const brillo::SecureBlob& owner_password);

  // Tries to connect to the TPM
  TSS_HCONTEXT ConnectContext();

  // Populates |context_handle| with a valid TSS_HCONTEXT and |tpm_handle| with
  // its matching TPM object iff the owner password is available and
  // authorization is successfully acquired.
  bool ConnectContextAsOwner(TSS_HCONTEXT* context_handle,
                             TSS_HTPM* tpm_handle);

  // Populates |context_handle| with a valid TSS_HCONTEXT and |tpm_handle| with
  // its matching TPM object authorized by the given delegation.
  bool ConnectContextAsDelegate(const brillo::SecureBlob& delegate_blob,
                                const brillo::SecureBlob& delegate_secret,
                                TSS_HCONTEXT* context, TSS_HTPM* tpm);

  // Populates |context_handle| with a valid TSS_HCONTEXT and |tpm_handle| with
  // its matching TPM object iff the context can be created and a TPM object
  // exists in the TSS.
  bool ConnectContextAsUser(TSS_HCONTEXT* context_handle,
                            TSS_HTPM* tpm_handle);

  // Returns the size of the specified NVRAM space.
  //
  // Parameters
  //   context_handle - The context handle for the TPM session
  //   index - NVRAM Space index
  // Returns -1 if the index, handle, or space is invalid.
  unsigned int GetNvramSizeForContext(TSS_HCONTEXT context_handle,
                                      TSS_HTPM tpm_handle,
                                      uint32_t index);

  // Returns if an Nvram space exists using the given context.
  bool IsNvramDefinedForContext(TSS_HCONTEXT context_handle,
                                TSS_HTPM tpm_handle,
                                uint32_t index);

  // Returns if bWriteDefine is true for a given NVRAM space using the given
  // context.
  bool IsNvramLockedForContext(TSS_HCONTEXT context_handle,
                               TSS_HTPM tpm_handle,
                               uint32_t index);

  // Reads an NVRAM space using the given context.
  bool ReadNvramForContext(TSS_HCONTEXT context_handle,
                           TSS_HTPM tpm_handle,
                           TSS_HPOLICY policy_handle,
                           uint32_t index,
                           brillo::SecureBlob* blob);

  // Gets a handle to the TPM from the specified context
  //
  // Parameters
  //   context_handle - The context handle for the TPM session
  //   tpm_handle (OUT) - The handle for the TPM on success
  bool GetTpm(TSS_HCONTEXT context_handle, TSS_HTPM* tpm_handle);

  // Gets a handle to the TPM from the specified context with the given owner
  // password
  //
  // Parameters
  //   context_handle - The context handle for the TPM session
  //   owner_password - The owner password to use when getting the handle
  //   tpm_handle (OUT) - The handle for the TPM on success
  bool GetTpmWithAuth(TSS_HCONTEXT context_handle,
                      const brillo::SecureBlob& owner_password,
                      TSS_HTPM* tpm_handle);

  // Gets a handle to the TPM from the specified context with the given
  // delegation.
  //
  // Parameters
  //   context_handle - The context handle for the TPM session
  //   delegate_blob - The delegate blob to use when getting the handle
  //   delegate_secret - The delegate secret to use when getting the handle
  //   tpm_handle (OUT) - The handle for the TPM on success
  bool GetTpmWithDelegation(TSS_HCONTEXT context_handle,
                            const brillo::SecureBlob& delegate_blob,
                            const brillo::SecureBlob& delegate_secret,
                            TSS_HTPM* tpm_handle);

  // Decrypts and parses an identity request.
  //
  // Parameters
  //   pca_key - The private key of the Privacy CA.
  //   request - The identity request data.
  //   identityBinding - The EK-AIK binding (i.e. public key signature).
  //   endorsementCredential - The endorsement credential.
  //   platformCredential - The platform credential.
  //   conformanceCredential - The conformance credential.
  bool DecryptIdentityRequest(RSA* pca_key, const brillo::SecureBlob& request,
                              brillo::SecureBlob* identity_binding,
                              brillo::SecureBlob* endorsement_credential,
                              brillo::SecureBlob* platform_credential,
                              brillo::SecureBlob* conformance_credential);

  // Creates a DER encoded RSA public key given a serialized TPM_PUBKEY.
  //
  // Parameters
  //   public_key - A serialized TPM_PUBKEY as returned by Tspi_Key_GetPubKey.
  //   public_key_der - The same public key in DER encoded form.
  bool ConvertPublicKeyToDER(const brillo::SecureBlob& public_key,
                             brillo::SecureBlob* public_key_der);

  // Wrapper for Tspi_GetAttribData.
  bool GetDataAttribute(TSS_HCONTEXT context,
                        TSS_HOBJECT object,
                        TSS_FLAG flag,
                        TSS_FLAG sub_flag,
                        brillo::SecureBlob* data) const;

  // Wrapper for Tspi_TPM_GetCapability. If |data| is not NULL, the raw
  // capability data will be assigned. If |value| is not NULL, the capability
  // data must be exactly 4 bytes and it will be decoded into |value|.
  bool GetCapability(TSS_HCONTEXT context_handle,
                     TSS_HTPM tpm_handle,
                     UINT32 capability,
                     UINT32 sub_capability,
                     brillo::Blob* data,
                     UINT32* value) const;

  // Member variables
  bool initialized_;
  brillo::SecureBlob srk_auth_;

  // If TPM ownership is taken, owner_password_ contains the password used
  brillo::SecureBlob owner_password_;

  // Used to provide thread-safe access to owner_password_, as it is set in the
  // initialization background thread.
  base::Lock password_sync_lock_;

  // Indicates if the TPM is disabled
  bool is_disabled_;

  // Indicates if the TPM is owned
  bool is_owned_;

  // Indicates if the TPM is being owned
  bool is_being_owned_;

  // Tpm Context information
  trousers::ScopedTssContext tpm_context_;

  DISALLOW_COPY_AND_ASSIGN(TpmImpl);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM_IMPL_H_
