// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tpm - class for performing encryption/decryption in the TPM.  For cryptohome,
// the TPM may be used as a way to strenghten the security of the wrapped vault
// keys stored on disk.  When the TPM is enabled, there is a system-wide
// cryptohome RSA key that is used during the encryption/decryption of these
// keys.

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
  //   password_rounds - The number of hash rounds to use creating a key from
  //                     the password
  //   salt - The salt used in converting the password to the UP
  //   data_out (OUT) - The encrypted data
  virtual bool Encrypt(const chromeos::Blob& data,
                       const chromeos::Blob& password, int password_rounds,
                       const chromeos::Blob& salt, SecureBlob* data_out) const;

  // Decrypts a data blob using the TPM cryptohome RSA key
  //
  // Parameters
  //   data - The encrypted data
  //   password - The password to use as the UP in the method described above
  //   password_rounds - The number of hash rounds to use creating a key from
  //                     the password
  //   salt - The salt used in converting the password to the UP
  //   data_out (OUT) - The decrypted data
  virtual bool Decrypt(const chromeos::Blob& data,
                       const chromeos::Blob& password, int password_rounds,
                       const chromeos::Blob& salt, SecureBlob* data_out) const;

  // Returns the maximum number of RSA keys that the TPM can hold simultaneously
  int GetMaxRsaKeyCount() const;

  // Retrieves the TPM-wrapped cryptohome RSA key
  bool GetKey(SecureBlob* blob) const;
  // Retrieves the Public key component of the cryptohome RSA key
  bool GetPublicKey(SecureBlob* blob) const;
  // Loads the cryptohome RSA key from the specified TPM-wrapped key
  bool LoadKey(const SecureBlob& blob);

  void set_srk_auth(const SecureBlob& value) {
    srk_auth_.resize(value.size());
    memcpy(srk_auth_.data(), value.const_data(), srk_auth_.size());
  }

  void set_crypto(Crypto* value) {
    crypto_ = value;
  }

  void set_rsa_key_bits(int value) {
    rsa_key_bits_ = value;
  }

  void set_key_file(const std::string& value) {
    key_file_ = value;
  }

 private:
  bool LoadSrk(TSS_HCONTEXT context_handle, TSS_HKEY* srk_handle) const;

  bool OpenAndConnectTpm(TSS_HCONTEXT* context_handle) const;

  int GetMaxRsaKeyCountForContext(TSS_HCONTEXT context_handle) const;

  bool LoadOrCreateCryptohomeKey(TSS_HCONTEXT context_handle,
                                 bool create_in_tpm,
                                 bool register_key,
                                 TSS_HKEY* key_handle);

  bool EncryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                   const chromeos::Blob& data, const chromeos::Blob& password,
                   int password_rounds, const chromeos::Blob& salt,
                   SecureBlob* data_out) const;

  bool DecryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                   const chromeos::Blob& data, const chromeos::Blob& password,
                   int password_rounds, const chromeos::Blob& salt,
                   SecureBlob* data_out) const;

  bool GetKeyBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                  SecureBlob* data_out) const;

  bool GetPublicKeyBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                        SecureBlob* data_out) const;

  bool LoadKeyBlob(TSS_HCONTEXT context_handle, const SecureBlob& blob,
                   TSS_HKEY* key_handle) const;

  int rsa_key_bits_;
  SecureBlob srk_auth_;
  Crypto* crypto_;
  TSS_HCONTEXT context_handle_;
  TSS_HKEY key_handle_;
  std::string key_file_;

  DISALLOW_COPY_AND_ASSIGN(Tpm);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM_H_
