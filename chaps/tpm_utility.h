// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TPM_UTILITY_H
#define CHAPS_TPM_UTILITY_H

namespace chaps {

// TPMUtility is a high-level interface to TPM services. In practice, only a
// single instance of this class is necessary to provide TPM services across
// multiple logical tokens and sessions.
// TODO(dkrahn): Fully define this interface.
class TPMUtility {
 public:
  virtual ~TPMUtility() {}
  // Performs initialization tasks including the loading of the storage root key
  // (SRK).
  //   srk_auth_data - Authorization data for the SRK (typically empty).
  virtual bool Init(const std::string& srk_auth_data) = 0;
  // Authenticates a user by decrypting the user's master key with the user's
  // authorization key.
  //   auth_data - The user's authorization data (which is derived from the
  //               the user's password).
  //   auth_key_blob - The authorization key blob as provided by the TPM when
  //                   the key was generated.
  //   encrypted_master_key - The master key encrypted with the authorization
  //                          key.
  //   master_key - Will be populated with the decrypted master key.
  virtual bool Authenticate(const std::string& auth_data,
                            const std::string& auth_key_blob,
                            const std::string& encrypted_master_key,
                            std::string* master_key) = 0;
  // Changes authorization data for a user's authorization key.
  virtual bool ChangeAuthData(const std::string& old_auth_data,
                              const std::string& new_auth_data,
                              const std::string& old_auth_key_blob,
                              std::string* new_auth_key_blob) = 0;
  virtual bool GenerateRandom(int num_bytes, std::string* random_data) = 0;
  // Generates a key in the TPM and wraps it with the SRK.
  //   auth_data - Authorization data which will be associated with the new key.
  //   key_blob - Will be populated with the wrapped key blob as provided by the
  //              TPM.
  virtual bool GenerateKey(const std::string& auth_data,
                           std::string* key_blob) = 0;
  // Performs a TPM 'bind' operation. This effectively performs PKCS #1 v1.5 RSA
  // encryption (using PKCS #1 'type 2' padding).
  //   key_blob - The wrapped key blob as previously provided by the TPM.
  //   auth_data - Authorization data previously associated with the key.
  //   input - Data to be encrypted. The length of this data must not exceed
  //           'N - 11' where N is the length in bytes of the RSA key modulus.
  //   output - The encrypted data. The length will always match the length of
  //            the RSA key modulus.
  virtual bool Bind(const std::string& key_blob,
                    const std::string& auth_data,
                    const std::string& input,
                    std::string* output) = 0;
};

}  // namespace

#endif  // CHAPS_TPM_UTILITY_H
