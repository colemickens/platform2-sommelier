// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CRYPTOLIB_H_
#define CRYPTOHOME_CRYPTOLIB_H_

#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <chromeos/secure_blob.h>

#include "attestation.pb.h"  // NOLINT(build/include)

namespace cryptohome {

extern const unsigned int kDefaultPasswordRounds;
extern const unsigned int kWellKnownExponent;
extern const unsigned int kAesBlockSize;

class CryptoLib {
 public:
  CryptoLib();
  ~CryptoLib();

  enum PaddingScheme {
    kPaddingNone = 0,
    // Also called PKCS padding.
    // See http://tools.ietf.org/html/rfc5652#section-6.3.
    kPaddingStandard = 1,
    kPaddingCryptohomeDefault = 2,
  };

  enum BlockMode {
    kEcb = 1,
    kCbc = 2,
  };

  static void GetSecureRandom(unsigned char *bytes, size_t len);
  static bool CreateRsaKey(size_t bits, chromeos::SecureBlob* n,
                           chromeos::SecureBlob* p);
  static chromeos::SecureBlob Sha1(const chromeos::Blob& data);
  static chromeos::SecureBlob Sha256(const chromeos::Blob& data);
  static chromeos::SecureBlob HmacSha512(const chromeos::SecureBlob& key,
                                         const chromeos::Blob& data);
  static chromeos::SecureBlob HmacSha256(const chromeos::SecureBlob& key,
                                         const chromeos::Blob& data);

  static size_t GetAesBlockSize();
  static bool PasskeyToAesKey(const chromeos::Blob& passkey,
                              const chromeos::Blob& salt,
                              unsigned int rounds,
                              chromeos::SecureBlob* key,
                              chromeos::SecureBlob* iv);

  // Decrypts data encrypted with AesEncrypt.
  //
  // Parameters
  //   wrapped - The blob containing the encrypted data
  //   key - The AES key to use in decryption
  //   iv - The initialization vector to use
  //   plaintext - The unwrapped (decrypted) data
  static bool AesDecrypt(const chromeos::Blob& ciphertext,
                        const chromeos::SecureBlob& key,
                        const chromeos::SecureBlob& iv,
                        chromeos::SecureBlob* plaintext);

  // AES encrypts the plain text data using the specified key and IV.  This
  // method uses custom padding and is not inter-operable with other crypto
  // systems.  The encrypted data can be decrypted with AesDecrypt.
  //
  // Parameters
  //   plaintext - The plain text data to encrypt
  //   key - The AES key to use
  //   iv - The initialization vector to use
  //   ciphertext - On success, the encrypted data
  static bool AesEncrypt(const chromeos::Blob& plaintext,
                         const chromeos::SecureBlob& key,
                         const chromeos::SecureBlob& iv,
                         chromeos::SecureBlob* ciphertext);

  // Same as AesDecrypt, but allows using either CBC or ECB
  static bool AesDecryptSpecifyBlockMode(const chromeos::Blob& ciphertext,
                                         unsigned int start, unsigned int count,
                                         const chromeos::SecureBlob& key,
                                         const chromeos::SecureBlob& iv,
                                         PaddingScheme padding, BlockMode mode,
                                         chromeos::SecureBlob* plaintext);

  // Same as AesEncrypt, but allows using either CBC or ECB
  static bool AesEncryptSpecifyBlockMode(const chromeos::Blob& plaintext,
                                         unsigned int start, unsigned int count,
                                         const chromeos::SecureBlob& key,
                                         const chromeos::SecureBlob& iv,
                                         PaddingScheme padding, BlockMode mode,
                                         chromeos::SecureBlob* ciphertext);

  // Encodes a binary blob to hex-ascii. Similar to base::HexEncode but
  // produces lowercase letters for hex digits.
  //
  // Parameters
  //   blob - The binary blob to convert
  static std::string BlobToHex(const chromeos::Blob& blob);
  // Parameters
  //   blob - The binary blob to convert
  //   buffer (IN/OUT) - Where to store the converted blob
  //   buffer_length - The size of the buffer
  static void BlobToHexToBuffer(const chromeos::Blob& blob,
                                void* buffer,
                                size_t buffer_length);

  // Encodes a binary blob to base64.
  //
  // Parameters
  //   blob - The input blob.
  //   include_newlines - Whether to include PEM-style newlines.
  static std::string Base64Encode(const std::string& blob,
                                  bool include_newlines);

  // Decodes a binary blob to base64.
  //
  // Parameters
  //   blob - The input blob.
  static std::string Base64Decode(const std::string& blob);

  // Computes an HMAC over the iv and encrypted_data fields of an EncryptedData
  // protobuf.
  // Parameters
  //   encrypted_data - encrypted data protobuf..
  //   hmac_key - secret key to use in hmac computation.
  static std::string ComputeEncryptedDataHMAC(
      const EncryptedData& encrypted_data,
      const chromeos::SecureBlob& hmac_key);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CRYPTOLIB_H_
