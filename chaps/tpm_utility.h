// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TPM_UTILITY_H_
#define CHAPS_TPM_UTILITY_H_

#include <string>

#include <brillo/secure_blob.h>

#include "chaps/chaps_utility.h"

namespace chaps {

// TPMUtility is a high-level interface to TPM services. In practice, only a
// single instance of this class is necessary to provide TPM services across
// multiple logical tokens and sessions.
class TPMUtility {
 public:
  virtual ~TPMUtility() {}

  // Returns the minimum supported RSA key size (in bits).
  virtual size_t MinRSAKeyBits() = 0;

  // Returns the maximum supported RSA key size (in bits).
  virtual size_t MaxRSAKeyBits() = 0;

  // Performs initialization tasks including the loading of the storage root key
  // (SRK). This may be called multiple times.
  // Returns true on success.
  virtual bool Init() = 0;

  // Returns true if a TPM exists and is enabled.
  virtual bool IsTPMAvailable() = 0;

  // Authenticates a user by decrypting the user's master key with the user's
  // authorization key.
  //   auth_data - The user's authorization data (which is derived from the
  //               the user's password).
  //   auth_key_blob - The authorization key blob as provided by the TPM when
  //                   the key was generated.
  //   encrypted_master_key - The master key encrypted with the authorization
  //                          key.
  //   master_key - Will be populated with the decrypted master key.
  // Returns true on success.
  virtual bool Authenticate(int slot_id,
                            const brillo::SecureBlob& auth_data,
                            const std::string& auth_key_blob,
                            const std::string& encrypted_master_key,
                            brillo::SecureBlob* master_key) = 0;

  // Changes authorization data for a user's authorization key. Returns true on
  // success.
  virtual bool ChangeAuthData(int slot_id,
                              const brillo::SecureBlob& old_auth_data,
                              const brillo::SecureBlob& new_auth_data,
                              const std::string& old_auth_key_blob,
                              std::string* new_auth_key_blob) = 0;

  // Provides hardware-generated random data. Returns true on success.
  virtual bool GenerateRandom(int num_bytes, std::string* random_data) = 0;

  // Adds entropy to the hardware random number generator. This is like seeding
  // the generator except the provided entropy is mixed with existing state and
  // the resulting random numbers generated are not deterministic. Returns true
  // on success.
  virtual bool StirRandom(const std::string& entropy_data) = 0;

  // Generates an RSA key pair in the TPM and wraps it with the SRK. The key
  // type will be set to TSS_KEY_TYPE_LEGACY.
  //   slot - The slot associated with this key.
  //   modulus_bits - The size of the key to be generated (usually 2048).
  //   public_exponent - The RSA public exponent (usually {1, 0, 1} which is
  //                     65537).
  //   auth_data - Authorization data which will be associated with the new key.
  //   key_blob - Will be populated with the wrapped key blob as provided by the
  //              TPM. This should be saved so the key can be loaded again
  //              in the future.
  //   key_handle - A handle to the new key. This will be valid until keys are
  //                unloaded for the given slot.
  // Returns true on success.
  virtual bool GenerateRSAKey(int slot,
                              int modulus_bits,
                              const std::string& public_exponent,
                              const brillo::SecureBlob& auth_data,
                              std::string* key_blob,
                              int* key_handle) = 0;

  // Retrieves the public components of an RSA key pair. Returns true on
  // success.
  virtual bool GetRSAPublicKey(int key_handle,
                               std::string* public_exponent,
                               std::string* modulus) = 0;

  // Return supports |curve_nid| or not. |curve_nid| is the NID of OpenSSL.
  // TPM 1.2 doesn't support ECC.
  // TPM 2.0 current only support P-256 curve (NID_X9_62_prime256v1).
  virtual bool IsECCurveSupported(int curve_nid) = 0;

  // Generates an ECC key pair in the TPM and wraps it with the SRK.
  //   slot - The slot associated with this key.
  //   nid - the OpenSSL NID for the curve.
  //   auth_data - Authorization data which will be associated with the new key.
  //   key_blob - Will be populated with the wrapped key blob as provided by the
  //              TPM. This should be saved so the key can be loaded again
  //              in the future.
  //   key_handle - A handle to the new key. This will be valid until keys are
  //                unloaded for the given slot.
  // Returns true on success.
  virtual bool GenerateECCKey(int slot,
                              int nid,
                              const brillo::SecureBlob& auth_data,
                              std::string* key_blob,
                              int* key_handle) = 0;

  // Retrieves the public point of ECC key pair. Returns true on success.
  //   key_handle - A TPM key handle
  //   public_point - A DER-encoded string of EC_Point.
  virtual bool GetECCPublicKey(int key_handle, std::string* public_point) = 0;

  // Wraps an RSA key pair with the SRK. The key type will be set to
  // TSS_KEY_TYPE_LEGACY.
  //   slot - The slot associated with this key.
  //   public_exponent - The RSA public exponent (e).
  //   modulus - The RSA modulus (n).
  //   prime_factor - One of the prime factors of the modulus (p or q).
  //   auth_data - Authorization data which will be associated with the new key.
  //   key_blob - Will be populated with the wrapped key blob as provided by the
  //              TPM. This should be saved so the key can be loaded again
  //              in the future.
  //   key_handle - A handle to the new key. This will be valid until keys are
  //                unloaded for the given slot.
  // Returns true on success.
  virtual bool WrapRSAKey(int slot,
                          const std::string& public_exponent,
                          const std::string& modulus,
                          const std::string& prime_factor,
                          const brillo::SecureBlob& auth_data,
                          std::string* key_blob,
                          int* key_handle) = 0;

  // Wraps an RSA key pair with the SRK.
  //   slot - The slot associated with this key.
  //   curve_nid - The OpenSSL NID of the ECC curve.
  //   public_point_x - The x coordinate of ECC public key point on the curve.
  //   public_point_y - The y coordinate of ECC public key point on the curve.
  //   private_value - The ECC private key value.
  //   auth_data - Authorization data which will be associated with the new key.
  //   key_blob - Will be populated with the wrapped key blob as provided by the
  //              TPM. This should be saved so the key can be loaded again
  //              in the future.
  //   key_handle - A handle to the new key. This will be valid until keys are
  //                unloaded for the given slot.
  // Returns true on success.
  virtual bool WrapECCKey(int slot,
                          int curve_nid,
                          const std::string& public_point_x,
                          const std::string& public_point_y,
                          const std::string& private_value,
                          const brillo::SecureBlob& auth_data,
                          std::string* key_blob,
                          int* key_handle) = 0;

  // Loads a key by blob into the TPM.
  //   slot - The slot associated with this key.
  //   key_blob - The key blob as provided by GenerateKey or WrapRSAKey.
  //   auth_data - Authorization data for the key.
  //   key_handle - A handle to the loaded key. This will be valid until keys
  //                are unloaded for the given slot.
  // Returns true on success.
  virtual bool LoadKey(int slot,
                       const std::string& key_blob,
                       const brillo::SecureBlob& auth_data,
                       int* key_handle) = 0;

  // Loads a key by blob into the TPM that has a parent key that is not the SRK.
  //   slot - The slot associated with this key.
  //   key_blob - The key blob as provided by GenerateKey or WrapRSAKey.
  //   auth_data - Authorization data for the key.
  //   parent_key_handle - The key handle of the parent key.
  //   key_handle - A handle to the loaded key. This will be valid until keys
  //                are unloaded for the given slot.
  // Returns true on success.
  virtual bool LoadKeyWithParent(int slot,
                                 const std::string& key_blob,
                                 const brillo::SecureBlob& auth_data,
                                 int parent_key_handle,
                                 int* key_handle) = 0;

  // Unloads all keys loaded for a particular slot. All key handles for the
  // given slot will not be valid after this method returns.
  virtual void UnloadKeysForSlot(int slot) = 0;

  // Performs a 'bind' operation using the TSS_ES_RSAESPKCSV15 scheme. This
  // effectively performs PKCS #1 v1.5 RSA encryption (using PKCS #1 'type 2'
  // padding).
  //   key_handle - The key handle, as provided by LoadKey, WrapRSAKey, or
  //                GenerateKey.
  //   input - Data to be encrypted. The length of this data must not exceed
  //           'N - 11' where N is the length in bytes of the RSA key modulus.
  //   output - The encrypted data. The length will always match the length of
  //            the RSA key modulus.
  // Returns true on success.
  virtual bool Bind(int key_handle,
                    const std::string& input,
                    std::string* output) = 0;

  // Performs a 'unbind' operation using the TSS_ES_RSAESPKCSV15 scheme. This
  // effectively performs PKCS #1 v1.5 RSA decryption (using PKCS #1 'type 2'
  // padding).
  //   key_handle - The key handle, as provided by LoadKey, WrapRSAKey or
  //                GenerateKey.
  //   input - Data to be encrypted. The length of this data must not exceed
  //           'N - 11' where N is the length in bytes of the RSA key modulus.
  //   output - The encrypted data. The length will always match the length of
  //            the RSA key modulus.
  // Returns true on success.
  virtual bool Unbind(int key_handle,
                      const std::string& input,
                      std::string* output) = 0;

  // Generates a digital signature.
  //   key_handle - The key handle, as provided by LoadKey, WrapRSAKey or
  //                GenerateKey.
  //   input - The raw data we want to sign. For RSASSA, the DER encoding of the
  //           DigestInfo value (see PKCS #1 v.2.1: 9.2) will be added
  //           internally.
  //   signature - Receives the generated signature. The signature length will
  //               always match the length of the RSA key modulus.
  // Returns true on success.
  virtual bool Sign(int key_handle,
                    DigestAlgorithm digest_algorithm,
                    const std::string& input,
                    std::string* signature) = 0;

  // Returns true iff the Storage Root Key is initialized and ready.  The SRK is
  // expected to not be ready until ownership of the TPM has been taken.
  virtual bool IsSRKReady() = 0;
};

}  // namespace chaps

#endif  // CHAPS_TPM_UTILITY_H_
