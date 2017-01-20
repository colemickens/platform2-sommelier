// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tpm - class for performing encryption/decryption in the TPM.  For cryptohome,
// the TPM may be used as a way to strengthen the security of the wrapped vault
// keys stored on disk.  When the TPM is enabled, there is a system-wide
// cryptohome RSA key that is used during the encryption/decryption of these
// keys.
// TODO(wad) make more functions virtual for use in mock_tpm.h.

#include <base/synchronization/lock.h>
#include <brillo/secure_blob.h>
#include <openssl/rsa.h>

#include "tpm_status.pb.h"  // NOLINT(build/include)

#if USE_TPM2
#include "cryptohome/tpm2.h"
#else
#include "cryptohome/tpm1.h"
#endif

#ifndef CRYPTOHOME_TPM_H_
#define CRYPTOHOME_TPM_H_


namespace cryptohome {

using TpmKeyHandle = uint32_t;

class Tpm;

const TpmKeyHandle kInvalidKeyHandle = 0;
const int kNotBoundToPCR = -1;

// This class provides a wrapper around TpmKeyHandle, and manages freeing of
// TPM reseources associated with TPM keys. It does not take ownership of the
// Tpm pointer provided.
class ScopedKeyHandle {
 public:
  ScopedKeyHandle();
  ~ScopedKeyHandle();
  TpmKeyHandle value();
  TpmKeyHandle release();
  void reset(Tpm* tpm, TpmKeyHandle handle);
 private:
  Tpm* tpm_;
  TpmKeyHandle handle_;
  DISALLOW_COPY_AND_ASSIGN(ScopedKeyHandle);
};

class Tpm {
 public:
  enum TpmVersion {
    TPM_UNKNOWN = 0,
    TPM_1_2 = 1,
    TPM_2_0 = 2,
  };

  enum TpmRetryAction {
    kTpmRetryNone,
    kTpmRetryFailNoRetry,
    kTpmRetryCommFailure,
    kTpmRetryDefendLock,
    kTpmRetryFatal,
    kTpmRetryInvalidHandle,
    kTpmRetryLoadFail,
    kTpmRetryReboot,
  };

  enum TpmNvramFlags {
    // NVRAM space is write-once; lock by writing 0 bytes
    kTpmNvramWriteDefine = (1<<0),

    // NVRAM space is only accessible if PCR0 has the same value it did
    // when the space was created
    kTpmNvramBindToPCR0 = (1<<1),
  };

  struct TpmStatusInfo {
    uint32_t last_tpm_error;
    bool can_connect;
    bool can_load_srk;
    bool can_load_srk_public_key;
    bool has_cryptohome_key;
    bool can_encrypt;
    bool can_decrypt;
    bool this_instance_has_context;
    bool this_instance_has_key_handle;
  };

  static Tpm* GetSingleton();

  static const uint32_t kLockboxIndex;

  virtual ~Tpm() {}

  // Returns TPM version
  virtual TpmVersion GetVersion() = 0;

  // Encrypts a data blob using the provided RSA key. Returns a TpmRetryAction
  // struct
  //
  // Parameters
  //   key_handle - The loaded TPM key handle
  //   plaintext - One RSA message to encrypt
  //   key - AES key to encrypt with
  //   ciphertext (OUT) - Encrypted blob
  virtual TpmRetryAction EncryptBlob(TpmKeyHandle key_handle,
                                     const brillo::SecureBlob& plaintext,
                                     const brillo::SecureBlob& key,
                                     brillo::SecureBlob* ciphertext) = 0;

  // Decrypts a data blob using the provided RSA key. Returns a TpmRetryAction
  // struct
  //
  // Parameters
  //   key_handle - The loaded TPM key handle
  //   ciphertext - One RSA message to encrypt
  //   key - AES key to encrypt with
  //   plaintext (OUT) - Decrypted blob
  virtual TpmRetryAction DecryptBlob(TpmKeyHandle key_handle,
                                     const brillo::SecureBlob& ciphertext,
                                     const brillo::SecureBlob& key,
                                     brillo::SecureBlob* plaintext) = 0;

  // Retrieves the sha1sum of the public key component of the RSA key
  virtual TpmRetryAction GetPublicKeyHash(TpmKeyHandle key_handle,
                                          brillo::SecureBlob* hash) = 0;

  // Returns the owner password if this instance was used to take ownership.
  // This will only occur when the TPM is unowned, which will be on OOBE
  //
  // Parameters
  //   owner_password (OUT) - The random owner password used
  virtual bool GetOwnerPassword(brillo::Blob* owner_password) = 0;


  // Returns whether or not the TPM is enabled.  This method call returns a
  // cached result because querying the TPM directly will block if ownership is
  // currently being taken (such as on a separate thread).
  virtual bool IsEnabled() = 0;
  virtual void SetIsEnabled(bool enabled) = 0;

  // Returns whether or not the TPM is owned.  This method call returns a cached
  // result because querying the TPM directly will block if ownership is
  // currently being taken (such as on a separate thread).
  virtual bool IsOwned() = 0;
  virtual void SetIsOwned(bool owned) = 0;

  // Returns whether or not the TPM is enabled and owned using a call to
  // Tspi_TPM_GetCapability.
  //
  // Unlike former functions, this function performs the check (which could take
  // some time) every time it is invoked. It does not use cached value.
  //
  // Parameters
  //   enabled (OUT) - Whether the TPM is enabled
  //   owned (OUT) - Whether the TPM is owned
  //
  // Returns true if the check was successfully carried out.
  virtual bool PerformEnabledOwnedCheck(bool* enabled, bool* owned) = 0;

  // Returns whether or not this instance has been setup'd by an external
  // entity (such as cryptohome::TpmInit).
  virtual bool IsInitialized() = 0;
  virtual void SetIsInitialized(bool done) = 0;

  // Returns whether or not the TPM is being owned
  virtual bool IsBeingOwned() = 0;
  virtual void SetIsBeingOwned(bool value) = 0;

  // Gets random bytes from the TPM
  //
  // Parameters
  //   length - The number of bytes to get
  //   data (OUT) - The random data from the TPM
  virtual bool GetRandomData(size_t length, brillo::Blob* data) = 0;

  // Creates a NVRAM space in the TPM
  //
  // Parameters
  //   index - The index of the space
  //   length - The number of bytes to allocate
  //   flags - Flags for NVRAM space attributes; zero or more TpmNvramFlags
  // Returns false if the index or length invalid or the required
  // authorization is not possible.
  virtual bool DefineNvram(uint32_t index,
                           size_t length,
                           uint32_t flags) = 0;

  // Destroys a defined NVRAM space
  //
  // Parameters
  //  index - The index of the space to destroy
  // Returns false if the index is invalid or the required authorization
  // is not possible.
  virtual bool DestroyNvram(uint32_t index) = 0;

  // Writes the given blob to NVRAM
  //
  // Parameters
  //  index - The index of the space to write
  //  blob - the data to write (size==0 may be used for locking)
  // Returns false if the index is invalid or the request lacks the required
  // authorization.
  virtual bool WriteNvram(uint32_t index,
                          const brillo::SecureBlob& blob) = 0;

  // Reads from the NVRAM index to the given blob
  //
  // Parameters
  //  index - The index of the space to write
  //  blob - the data to read
  // Returns false if the index is invalid or the request lacks the required
  // authorization.
  virtual bool ReadNvram(uint32_t index, brillo::SecureBlob* blob) = 0;

  // Determines if the given index is defined in the TPM
  //
  // Parameters
  //  index - The index of the space
  // Returns true if it exists and false if it doesn't or there is a failure to
  // communicate with the TPM.
  virtual bool IsNvramDefined(uint32_t index) = 0;

  // Determines if the NVRAM space at the given index is bWriteDefine locked
  //
  // Parameters
  //   index - The index of the space
  // Returns true if locked and false if it is unlocked, the space does not
  // exist, or there is a TPM-related error.
  virtual bool IsNvramLocked(uint32_t index) = 0;

  // Locks NVRAM space for writing
  //
  // Parameters
  //  index - The index of the space
  // Returns true if the index has been successfully write-locked, and false
  // otherwise.
  virtual bool WriteLockNvram(uint32_t index) = 0;

  // Returns the reported size of the NVRAM space indicated by its index
  //
  // Parameters
  //   index - The index of the space
  // Returns the size of the space. If undefined or an error occurs, 0 is
  // returned.
  virtual unsigned int GetNvramSize(uint32_t index) = 0;

  // Get the endorsement public key. This method requires TPM owner privilege.
  //
  // Parameters
  //   ek_public_key - The EK public key in DER encoded form.
  //
  // Returns true on success.
  virtual bool GetEndorsementPublicKey(brillo::SecureBlob* ek_public_key) = 0;

  // Get the endorsement credential. This method requires TPM owner privilege.
  //
  // Parameters
  //   credential - The EK credential as it is stored in NVRAM.
  //
  // Returns true on success.
  virtual bool GetEndorsementCredential(brillo::SecureBlob* credential) = 0;

  // Creates an Attestation Identity Key (AIK). This method requires TPM owner
  // privilege.
  //
  // Parameters
  //   identity_public_key_der - The AIK public key in DER encoded form.
  //   identity_public_key - The AIK public key in serialized TPM_PUBKEY form.
  //   identity_key_blob - The AIK key in blob form.
  //   identity_binding - The EK-AIK binding (i.e. public key signature).
  //   identity_label - The label used to create the identity binding.
  //   pca_public_key - The public key of the temporary PCA used to create the
  //                    identity binding in serialized TPM_PUBKEY form.
  //   endorsement_credential - The endorsement credential.
  //   platform_credential - The platform credential.
  //   conformance_credential - The conformance credential.
  //
  // Returns true on success.
  virtual bool MakeIdentity(brillo::SecureBlob* identity_public_key_der,
                            brillo::SecureBlob* identity_public_key,
                            brillo::SecureBlob* identity_key_blob,
                            brillo::SecureBlob* identity_binding,
                            brillo::SecureBlob* identity_label,
                            brillo::SecureBlob* pca_public_key,
                            brillo::SecureBlob* endorsement_credential,
                            brillo::SecureBlob* platform_credential,
                            brillo::SecureBlob* conformance_credential) = 0;

  // Generates a quote of a given PCR with the given identity key.
  // - PCR0 is used to differentiate normal mode from developer mode.
  // - PCR1 is used on some systems to measure the HWID.
  //
  // Parameters
  //   pcr_index - The index of the PCR to be quoted.
  //   identity_key_blob - The AIK blob, as provided by MakeIdentity.
  //   external_data - Data to be added to the quote, this must be at least 160
  //                   bits in length and only the first 160 bits will be used.
  //   pcr_value - The value of PCR0 at the time of quote. This is more reliable
  //               than separately reading the PCR value because it is not
  //               susceptible to race conditions.
  //   quoted_data - The exact data that was quoted (i.e. the TPM_QUOTE_INFO
  //                 structure), this can make verifying the quote easier.
  //   quote - The generated quote.
  //
  // Returns true on success.
  virtual bool QuotePCR(int pcr_index,
                        const brillo::SecureBlob& identity_key_blob,
                        const brillo::SecureBlob& external_data,
                        brillo::SecureBlob* pcr_value,
                        brillo::SecureBlob* quoted_data,
                        brillo::SecureBlob* quote) = 0;

  // Seals a secret to PCR0 with the SRK.
  //
  // Parameters
  //   value - The value to be sealed.
  //   sealed_value - The sealed value.
  //
  // Returns true on success.
  virtual bool SealToPCR0(const brillo::Blob& value,
                          brillo::Blob* sealed_value) = 0;

  // Unseals a secret previously sealed with the SRK.
  //
  // Parameters
  //   sealed_value - The sealed value.
  //   value - The original value.
  //
  // Returns true on success.
  virtual bool Unseal(const brillo::Blob& sealed_value,
                      brillo::Blob* value) = 0;

  // Creates a certified non-migratable signing key.
  //
  // Parameters
  //   identity_key_blob - The AIK key blob, as provided by MakeIdentity.
  //   certified_public_key - The certified public key in TPM_PUBKEY form.
  //   certified_public_key_der - The certified public key in DER encoded form.
  //   certified_key_blob - The certified key in blob form.
  //   certified_key_info - The key info that was signed (TPM_CERTIFY_INFO).
  //   certified_key_proof - The signature of the certified key info by the AIK.
  virtual bool CreateCertifiedKey(
      const brillo::SecureBlob& identity_key_blob,
      const brillo::SecureBlob& external_data,
      brillo::SecureBlob* certified_public_key,
      brillo::SecureBlob* certified_public_key_der,
      brillo::SecureBlob* certified_key_blob,
      brillo::SecureBlob* certified_key_info,
      brillo::SecureBlob* certified_key_proof) = 0;

  // Creates a TPM owner delegate for future use.
  //
  // Parameters
  //   identity_key_blob - The AIK key blob, as provided by MakeIdentity.
  //   delegate_blob - The blob for the owner delegation.
  //   delegate_secret - The delegate secret that will be required to perform
  //                     privileged operations in the future.
  virtual bool CreateDelegate(const brillo::SecureBlob& identity_key_blob,
                              brillo::SecureBlob* delegate_blob,
                              brillo::SecureBlob* delegate_secret) = 0;

  // Activates an AIK by using the EK to decrypt the AIK credential.
  //
  // Parameters
  //
  //   delegate_blob - The delegate blob, as provided by CreateDelegate.
  //   delegate_secret - The secret to be used for delegate authorization.
  //   identity_key_blob - The AIK key blob, as provided by MakeIdentity.
  //   encrypted_asym_ca - Encrypted TPM_ASYM_CA_CONTENTS from the CA.
  //   encrypted_sym_ca - Encrypted TPM_SYM_CA_CONTENTS from the CA.
  //   identity_credential - The AIK credential created by the CA.
  virtual bool ActivateIdentity(const brillo::SecureBlob& delegate_blob,
                                const brillo::SecureBlob& delegate_secret,
                                const brillo::SecureBlob& identity_key_blob,
                                const brillo::SecureBlob& encrypted_asym_ca,
                                const brillo::SecureBlob& encrypted_sym_ca,
                                brillo::SecureBlob* identity_credential) = 0;

  // Signs data using the TPM_SS_RSASSAPKCS1v15_DER scheme.  This method will
  // work with any signing key that has been assigned this scheme.  This
  // includes all keys created using CreateCertifiedKey.
  //
  // Parameters
  //   key_blob - An SRK-wrapped private key blob.
  //   input - The value to be signed.
  //   bound_pcr_index - If the signing key used is a PCR bound key, this arg
  //                     is the pcr to which it was bound. Else it is
  //                     kNotBoundToPCR.
  //   signature - On success, will be populated with the signature.
  virtual bool Sign(const brillo::SecureBlob& key_blob,
                    const brillo::SecureBlob& input,
                    int bound_pcr_index,
                    brillo::SecureBlob* signature) = 0;

  // Creates an SRK-wrapped signing key that has both create attributes and
  // usage policy bound to the given |pcr_index| and |pcr_value|.  On success
  // returns true and populates |key_blob| with the TPM private key blob and
  // |public_key_der| with the DER-encoded public key. |creation_blob| is an
  // opaque blob that must be passed back as an input into VerifyPCRBoundKey.
  virtual bool CreatePCRBoundKey(int pcr_index,
                                 const brillo::SecureBlob& pcr_value,
                                 brillo::SecureBlob* key_blob,
                                 brillo::SecureBlob* public_key_der,
                                 brillo::SecureBlob* creation_blob) = 0;

  // Returns true iff the given |key_blob| represents a SRK-wrapped key which
  // has both create attributes and usage policy bound to |pcr_value| for
  // |pcr_index|. |creation_blob| is the blob containing creation data, that
  // was generated by CreatePCRBoundKey.
  virtual bool VerifyPCRBoundKey(int pcr_index,
                                 const brillo::SecureBlob& pcr_value,
                                 const brillo::SecureBlob& key_blob,
                                 const brillo::SecureBlob& creation_blob) = 0;

  // Extends the PCR given by |pcr_index| with |extension|. The |extension| must
  // be exactly 20 bytes in length.
  virtual bool ExtendPCR(int pcr_index,
                         const brillo::SecureBlob& extension) = 0;

  // Reads the current |pcr_value| of the PCR given by |pcr_index|.
  virtual bool ReadPCR(int pcr_index, brillo::SecureBlob* pcr_value) = 0;

  // Checks to see if the endorsement key is available by attempting to get its
  // public key
  virtual bool IsEndorsementKeyAvailable() = 0;

  // Attempts to create the endorsement key in the TPM
  virtual bool CreateEndorsementKey() = 0;

  // Attempts to take ownership of the TPM
  //
  // Parameters
  //   max_timeout_tries - The maximum number of attempts to make if the call
  //                       times out, which it may occasionally do
  virtual bool TakeOwnership(int max_timeout_tries,
                             const brillo::SecureBlob& owner_password) = 0;

  // Initializes the SRK by Zero-ing its password and unrestricting it.
  //
  // Parameters
  //   owner_password - The owner password for the TPM
  virtual bool InitializeSrk(const brillo::SecureBlob& owner_password) = 0;

  // Changes the owner password
  //
  // Parameters
  //   previous_owner_password - The previous owner password for the TPM
  //   owner_password - The owner password for the TPM
  virtual bool ChangeOwnerPassword(
      const brillo::SecureBlob& previous_owner_password,
      const brillo::SecureBlob& owner_password) = 0;

  // Test the TPM auth by calling Tspi_TPM_GetStatus
  //
  // Parameters
  //   owner_password - The owner password to use when getting the handle
  virtual bool TestTpmAuth(const brillo::SecureBlob& owner_password) = 0;

  // Sets the TPM owner password to be used in subsequent commands
  //
  // Parameters
  //   owner_password - The owner password for the TPM
  virtual void SetOwnerPassword(const brillo::SecureBlob& owner_password) = 0;

  virtual bool IsTransient(TpmRetryAction retry_action) = 0;

  // Wrapps a provided RSA key with the TPM's Storage Root Key.
  //
  // Parameters
  //   public_modulus - the public modulus of the provided Rsa key
  //   prime_factor - one of the prime factors of the Rsa key to wrap
  //   wrapped_key (OUT) - A blob representing the wrapped key
  virtual bool WrapRsaKey(const brillo::SecureBlob& public_modulus,
                          const brillo::SecureBlob& prime_factor,
                          brillo::SecureBlob* wrapped_key) = 0;

  // Loads an SRK-wrapped key into the TPM.
  //
  // Parameters
  //   wrapped_key - The blob (as produced by WrapRsaKey).
  //   key_handle (OUT) - A handle to the key loaded into the TPM.
  virtual TpmRetryAction LoadWrappedKey(const brillo::SecureBlob& wrapped_key,
                                        ScopedKeyHandle* key_handle) = 0;

  // Loads the Cryptohome Key using a pre-defined UUID. This method does
  // nothing when using TPM2.0
  //
  // Parameters
  //   key_handle (OUT) - A handle to the key loaded into the TPM.
  //   key_blob (OUT) - If non-null, the blob representing this loaded key.
  virtual bool LegacyLoadCryptohomeKey(ScopedKeyHandle* key_handle,
                                       brillo::SecureBlob* key_blob) = 0;

  // Closes the TPM state associated with the given |key_handle|.
  virtual void CloseHandle(TpmKeyHandle key_handle) = 0;

  // Gets the TPM status information. If there |context| and |key| are supplied,
  // they will be used in encryption/decryption test. They can be 0 to bypass
  // the test.
  //
  // Parameters
  //   key - The key to check for encryption/decryption
  //   status (OUT) - The TpmStatusInfo structure containing the results
  virtual void GetStatus(TpmKeyHandle key, TpmStatusInfo* status) = 0;

  // Gets the current state of the dictionary attack logic. Returns false on
  // failure.
  virtual bool GetDictionaryAttackInfo(int* counter,
                                       int* threshold,
                                       bool* lockout,
                                       int* seconds_remaining) = 0;

  // Requires owner permissions so a |delegate_blob| and |delegate_secret| for
  // an owner delegate must be provided.
  virtual bool ResetDictionaryAttackMitigation(
      const brillo::SecureBlob& delegate_blob,
      const brillo::SecureBlob& delegate_secret) = 0;

  // For TPMs with updateable firmware: Declate the current firmware
  // version stable and invalidate previous versions, if any.
  // For TPMs with fixed firmware: NOP.
  virtual void DeclareTpmFirmwareStable() = 0;

 private:
  static Tpm* singleton_;
  static base::Lock singleton_lock_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM_H_
