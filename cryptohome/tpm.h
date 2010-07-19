// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tpm - class for performing encryption/decryption in the TPM.  For cryptohome,
// the TPM may be used as a way to strenghten the security of the wrapped vault
// keys stored on disk.  When the TPM is enabled, there is a system-wide
// cryptohome RSA key that is used during the encryption/decryption of these
// keys.  The method when TPM is enabled:
//
//   UP -
//       |
//       + AES (no padding) => IEVKK -
//       |                            |
// EVKK -                             |
//                                    + RSA (in TPM) => VKK
//                                    |
//                                    |
//                           TPM_CHK -
//
// Where:
//   UP - User Passkey
//   EVKK - Ecrypted vault keyset key (stored on disk)
//   IEVKK - Intermediate vault keyset key
//   TPM_CHK - TPM-wrapped system-wide Cryptohome Key
//   VKK - Vault Keyset Key
//
// The end result, the Vault Keyset Key (VKK), is an AES key that is used to
// decrypt the Vault Keyset, which holds the ecryptfs keys (filename encryption
// key and file encryption key).
//
// The User Passkey (UP) is used as an AES key to do an initial decrypt of the
// encrypted "tpm_key" field in the SerializedVaultKeyset (see
// vault_keyset.proto).  This is done without padding as the decryption is done
// in-place and the resulting buffer is fed into an RSA decrypt on the TPM as
// the cipher text.  That RSA decrypt uses the system-wide TPM-wrapped
// cryptohome key.  In this manner, we can use a randomly-created system-wide
// key (the TPM has a limited number of key slots), but still require the user's
// passkey during the decryption phase.  This also increases the brute-force
// cost of attacking the SerializedVaultKeyset offline as it means that the
// attacker would have to do a TPM cipher operation per password attempt
// (assuming that the wrapped key could not be recovered).
//
// After obtaining the VKK, the method is:
//
// VKK -
//      |
//      + AES (PKCS#5 padding + SHA1 verification) => VK
//      |
// EVK -
//
// By comparison, when the TPM is not enabled, the UP is used as the VKK, and
// the decryption of the Vault Keyset (VK) is merely the process above.

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <chromeos/utility.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>

#include "secure_blob.h"

#ifndef CRYPTOHOME_TPM_H_
#define CRYPTOHOME_TPM_H_

namespace cryptohome {

class Crypto;

extern const TSS_UUID kCryptohomeWellKnownUuid;

class Tpm {
 public:

  // Default constructor
  Tpm();

  virtual ~Tpm();

  // Initializes the Tpm instance
  //
  // Parameters
  //   crypto - The Crypto instance to use (for generating an RSA key, entropy,
  //            etc.)
  //   open_key - Whether or not to open (load) the cryptohome TPM key
  virtual bool Init(Crypto* crypto, bool open_key);

  // Tries to connect to the TPM
  virtual bool Connect();

  // Returns true if this instance is connected to the TPM
  virtual bool IsConnected() const;

  // Disconnects from the TPM
  virtual void Disconnect();

  // Encrypts a data blob using the TPM cryptohome RSA key
  //
  // Parameters
  //   data - The data to encrypt (must be small enough to fit into a single RSA
  //          decrypt--Tpm does not do blocking)
  //   password - The password to use as the UP in the method described above
  //   salt - The salt used in converting the password to the UP
  //   data_out (OUT) - The encrypted data
  virtual bool Encrypt(const chromeos::Blob& data,
                       const chromeos::Blob& password,
                       const chromeos::Blob& salt, SecureBlob* data_out) const;

  // Decrypts a data blob using the TPM cryptohome RSA key
  //
  // Parameters
  //   data - The encrypted data
  //   password - The password to use as the UP in the method described above
  //   salt - The salt used in converting the password to the UP
  //   data_out (OUT) - The decrypted data
  virtual bool Decrypt(const chromeos::Blob& data,
                       const chromeos::Blob& password,
                       const chromeos::Blob& salt, SecureBlob* data_out) const;

  // Returns the maximum number of RSA keys that the TPM can hold simultaneously
  int GetMaxRsaKeyCount() const;

  // Retrieves the TPM-wrapped cryptohome RSA key
  bool GetKey(SecureBlob* blob) const;
  // Loads the cryptohome RSA key from the specified TPM-wrapped key
  bool LoadKey(const SecureBlob& blob);

  void set_srk_auth(const SecureBlob& value) {
    srk_auth_.resize(value.size());
    memcpy(srk_auth_.data(), value.const_data(), srk_auth_.size());
  }

  void set_well_known_uuid(const TSS_UUID& value) {
    memcpy(&well_known_uuid_, &value, sizeof(TSS_UUID));
  }

  void set_crypto(Crypto* value) {
    crypto_ = value;
  }

  void set_rsa_key_bits(int value) {
    rsa_key_bits_ = value;
  }

 private:
  bool LoadSrk(TSS_HCONTEXT context_handle, TSS_HKEY* srk_handle) const;

  bool OpenAndConnectTpm(TSS_HCONTEXT* context_handle) const;

  int GetMaxRsaKeyCountForContext(TSS_HCONTEXT context_handle) const;

  bool RemoveCryptohomeKey(TSS_HCONTEXT context_handle,
                           const TSS_UUID& key_uuid) const;

  bool LoadOrCreateCryptohomeKey(TSS_HCONTEXT context_handle,
                                 const TSS_UUID& key_uuid, bool create_in_tpm,
                                 bool register_key, TSS_HKEY* key_handle) const;

  bool EncryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                   const chromeos::Blob& data, const chromeos::Blob& password,
                   const chromeos::Blob& salt, SecureBlob* data_out) const;

  bool DecryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                   const chromeos::Blob& data, const chromeos::Blob& password,
                   const chromeos::Blob& salt, SecureBlob* data_out) const;

  bool GetKeyBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                  SecureBlob* data_out) const;

  bool LoadKeyBlob(TSS_HCONTEXT context_handle, const SecureBlob& blob,
                   TSS_HKEY* key_handle) const;

  int rsa_key_bits_;
  SecureBlob srk_auth_;
  TSS_UUID well_known_uuid_;
  Crypto* crypto_;
  TSS_HCONTEXT context_handle_;
  TSS_HKEY key_handle_;

  DISALLOW_COPY_AND_ASSIGN(Tpm);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM_H_
