// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CRYPTOLIB_H_
#define CRYPTOHOME_CRYPTOLIB_H_

#include <base/basictypes.h>
#include <base/file_path.h>
#include <chromeos/secure_blob.h>

namespace cryptohome {

extern const unsigned int kDefaultPasswordRounds;
extern const unsigned int kWellKnownExponent;

class CryptoLib {
 public:
  CryptoLib();
  ~CryptoLib();

  enum PaddingScheme {
    kPaddingNone = 0,
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

  static size_t GetAesBlockSize();
  static bool PasskeyToAesKey(const chromeos::Blob& passkey,
                              const chromeos::Blob& salt,
                              unsigned int rounds,
                              chromeos::SecureBlob* key,
                              chromeos::SecureBlob* iv);

  // AES decrypts the wrapped blob
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

  // AES encrypts the plain text data using the specified key
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

  // Encodes a binary blob to hex-ascii
  //
  // Parameters
  //   blob - The binary blob to convert
  //   buffer (IN/OUT) - Where to store the converted blob
  //   buffer_length - The size of the buffer
  static void AsciiEncodeToBuffer(const chromeos::Blob& blob, char* buffer,
                                  unsigned int buffer_length);
};

}  // namespace cryptohome

#endif  // !CRYPTOHOME_CRYPTOLIB_H_
