// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptolib.h"

#include <limits>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <unistd.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/utility.h>
extern "C" {
#include <scrypt/crypto_scrypt.h>
#include <scrypt/scryptenc.h>
}

#include "platform.h"

using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

// The OpenSSL "magic" value that prefixes the encrypted data blob.  This is
// only used in migrating legacy keysets, which were stored in this manner.
const std::string kOpenSSLMagic = "Salted__";

// The well-known exponent used when generating RSA keys.  Cryptohome only
// generates one RSA key, which is the system-wide cryptohome key.  This is the
// common public exponent.
const unsigned int kWellKnownExponent = 65537;

// An upper bound on the amount of memory that we allow Scrypt to use when
// performing key strengthening (32MB).  A large size is okay since we only use
// Scrypt during the login process, before the user is logged in.  This memory
// is managed (and freed) by the scrypt library.
const unsigned int kScryptMaxMem = 32 * 1024 * 1024;

// An upper bound on the amount of time we allow Scrypt to use when performing
// key strenthening (1/3s) for encryption.
const double kScryptMaxEncryptTime = 0.333;

// An upper bound on the amount of time we allow Scrypt to use when performing
// key strenthening (100s) for decryption.  This number can be high because in
// practice, it doesn't mean much.  It simply needs to be large enough that we
// can guarantee that the key derived during encryption can always be derived at
// decryption time, so the typical time is usually close to 1/3s.  However,
// because sometimes other processes may interfere, we need it to be large
// enough to allow the same calculation to be made amidst other heavy use.
const double kScryptMaxDecryptTime = 100.0;

// Scrypt creates a header in the cipher text that we need to account for in
// buffer sizing.
const unsigned int kScryptHeaderLength = 128;

// The number of hash rounds we originally used when converting a password to a
// key.  This is used when converting older cryptohome vault keysets.
const unsigned int kDefaultLegacyPasswordRounds = 1;

// The current number of hash rounds we use.  Large enough to be a measurable
// amount of time, but not add too much overhead to login (around 10ms).
const unsigned int kDefaultPasswordRounds = 1337;

// AES key size in bytes (256-bit).  This key size is used for all key creation,
// though we currently only use 128 bits for the eCryptfs File Encryption Key
// (FEK).  Larger than 128-bit has too great of a CPU overhead on unaccelerated
// architectures.
const unsigned int kDefaultAesKeySize = 32;

// Maximum size of the salt file.
const int64 kSaltMax = (1 << 20);  // 1 MB

void CryptoLib::GetSecureRandom(unsigned char* buf, size_t length) {
  // OpenSSL takes a signed integer.  On the off chance that the user requests
  // something too large, truncate it.
  if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
    length = std::numeric_limits<int>::max();
  }
  RAND_bytes(buf, length);
}

bool CryptoLib::CreateRsaKey(size_t key_bits,
                             SecureBlob* n,
                          SecureBlob* p) {
  RSA* rsa = RSA_generate_key(key_bits, kWellKnownExponent, NULL, NULL);

  if (rsa == NULL) {
    LOG(ERROR) << "RSA key generation failed.";
    return false;
  }

  SecureBlob local_n(BN_num_bytes(rsa->n));
  if (BN_bn2bin(rsa->n, static_cast<unsigned char*>(local_n.data())) <= 0) {
    LOG(ERROR) << "Unable to get modulus from RSA key.";
    RSA_free(rsa);
    return false;
  }

  SecureBlob local_p(BN_num_bytes(rsa->p));
  if (BN_bn2bin(rsa->p, static_cast<unsigned char*>(local_p.data())) <= 0) {
    LOG(ERROR) << "Unable to get private key from RSA key.";
    RSA_free(rsa);
    return false;
  }

  RSA_free(rsa);
  n->swap(local_n);
  p->swap(local_p);
  return true;
}

SecureBlob CryptoLib::Sha1(const chromeos::Blob& data) {
  SHA_CTX sha_context;
  unsigned char md_value[SHA_DIGEST_LENGTH];
  SecureBlob hash;

  SHA1_Init(&sha_context);
  SHA1_Update(&sha_context, &data[0], data.size());
  SHA1_Final(md_value, &sha_context);
  hash.resize(sizeof(md_value));
  memcpy(hash.data(), md_value, sizeof(md_value));
  // Zero the stack to match expectations set by SecureBlob.
  chromeos::SecureMemset(md_value, 0, sizeof(md_value));
  return hash;
}

SecureBlob CryptoLib::Sha256(const chromeos::Blob& data) {
  SHA256_CTX sha_context;
  unsigned char md_value[SHA256_DIGEST_LENGTH];
  SecureBlob hash;

  SHA256_Init(&sha_context);
  SHA256_Update(&sha_context, &data[0], data.size());
  SHA256_Final(md_value, &sha_context);
  hash.resize(sizeof(md_value));
  memcpy(hash.data(), md_value, sizeof(md_value));
  // Zero the stack to match expectations set by SecureBlob.
  chromeos::SecureMemset(md_value, 0, sizeof(md_value));
  return hash;
}


size_t CryptoLib::GetAesBlockSize() {
  return EVP_CIPHER_block_size(EVP_aes_256_cbc());
}

bool CryptoLib::PasskeyToAesKey(const chromeos::Blob& passkey,
                                const chromeos::Blob& salt, unsigned int rounds,
                                SecureBlob* key, SecureBlob* iv) {
  if (salt.size() != PKCS5_SALT_LEN) {
    LOG(ERROR) << "Bad salt size.";
    return false;
  }

  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  SecureBlob aes_key(EVP_CIPHER_key_length(cipher));
  SecureBlob local_iv(EVP_CIPHER_iv_length(cipher));

  // Convert the passkey to a key
  if (!EVP_BytesToKey(cipher,
                      EVP_sha1(),
                      &salt[0],
                      &passkey[0],
                      passkey.size(),
                      rounds,
                      static_cast<unsigned char*>(aes_key.data()),
                      static_cast<unsigned char*>(local_iv.data()))) {
    LOG(ERROR) << "Failure converting bytes to key";
    return false;
  }

  key->swap(aes_key);
  if (iv) {
    iv->swap(local_iv);
  }

  return true;
}

bool CryptoLib::AesEncrypt(const chromeos::Blob& plaintext,
                           const SecureBlob& key,
                           const SecureBlob& iv,
                           SecureBlob* ciphertext) {
  return AesEncryptSpecifyBlockMode(plaintext, 0, plaintext.size(), key, iv,
                                    kPaddingCryptohomeDefault, kCbc,
                                    ciphertext);
}

bool CryptoLib::AesDecrypt(const chromeos::Blob& ciphertext,
                           const SecureBlob& key,
                           const SecureBlob& iv,
                           SecureBlob* plaintext) {
  return AesDecryptSpecifyBlockMode(ciphertext, 0, ciphertext.size(), key, iv,
                                    kPaddingCryptohomeDefault, kCbc, plaintext);
}

// This is the reverse operation of AesEncryptSpecifyBlockMode above.  See that
// method for a description of how padding and block_mode affect the crypto
// operations.  This method automatically removes and verifies the padding, so
// plain_text (on success) will contain the original data.
//
// Note that a call to AesDecryptSpecifyBlockMode needs to have the same padding
// and block_mode as the corresponding encrypt call.  Changing the block mode
// will drastically alter the decryption.  And an incorrect PaddingScheme will
// result in the padding verification failing, for which the method call fails,
// even if the key and initialization vector were correct.
bool CryptoLib::AesDecryptSpecifyBlockMode(const chromeos::Blob& encrypted,
                                           unsigned int start,
                                           unsigned int count,
                                           const SecureBlob& key,
                                           const SecureBlob& iv,
                                           PaddingScheme padding,
                                           BlockMode block_mode,
                                           SecureBlob* plain_text) {
  if ((start > encrypted.size()) ||
      ((start + count) > encrypted.size()) ||
      ((start + count) < start)) {
    return false;
  }
  SecureBlob local_plain_text(count);

  if (local_plain_text.size() >
      static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    // EVP_DecryptUpdate takes a signed int
    return false;
  }
  int final_size = 0;
  int decrypt_size = local_plain_text.size();

  const EVP_CIPHER* cipher;
  switch (block_mode) {
    case kCbc:
      cipher = EVP_aes_256_cbc();
      break;
    case kEcb:
      cipher = EVP_aes_256_ecb();
      break;
    default:
      LOG(ERROR) << "Invalid block mode specified: " << block_mode;
      return false;
  }
  if (key.size() != static_cast<unsigned int>(EVP_CIPHER_key_length(cipher))) {
    LOG(ERROR) << "Invalid key length of " << key.size()
               << ", expected " << EVP_CIPHER_key_length(cipher);
    return false;
  }
  // ECB ignores the IV, so only check the IV length if we are using a different
  // block mode.
  if ((block_mode != kEcb) &&
      (iv.size() != static_cast<unsigned int>(EVP_CIPHER_iv_length(cipher)))) {
    LOG(ERROR) << "Invalid iv length of " << iv.size()
               << ", expected " << EVP_CIPHER_iv_length(cipher);
    return false;
  }

  EVP_CIPHER_CTX decryption_context;
  EVP_CIPHER_CTX_init(&decryption_context);
  EVP_DecryptInit_ex(&decryption_context, cipher, NULL,
                     static_cast<const unsigned char*>(key.const_data()),
                     static_cast<const unsigned char*>(iv.const_data()));
  if (padding == kPaddingNone) {
    EVP_CIPHER_CTX_set_padding(&decryption_context, 0);
  }
  if (!EVP_DecryptUpdate(&decryption_context, &local_plain_text[0],
                         &decrypt_size, &encrypted[start], count)) {
    LOG(ERROR) << "DecryptUpdate failed";
    EVP_CIPHER_CTX_cleanup(&decryption_context);
    return false;
  }
  if (!EVP_DecryptFinal_ex(&decryption_context, &local_plain_text[decrypt_size],
                           &final_size)) {
    unsigned long err = ERR_get_error(); // NOLINT openssl types
    ERR_load_ERR_strings();
    ERR_load_crypto_strings();

    LOG(ERROR) << "DecryptFinal Error: " << err
               << ": " << ERR_lib_error_string(err)
               << ", " << ERR_func_error_string(err)
               << ", " << ERR_reason_error_string(err);

    EVP_CIPHER_CTX_cleanup(&decryption_context);
    return false;
  }
  final_size += decrypt_size;

  if (padding == kPaddingCryptohomeDefault) {
    if (final_size < SHA_DIGEST_LENGTH) {
      LOG(ERROR) << "Plain text was too small.";
      EVP_CIPHER_CTX_cleanup(&decryption_context);
      return false;
    }

    final_size -= SHA_DIGEST_LENGTH;

    SHA_CTX sha_context;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&sha_context);
    SHA1_Update(&sha_context, &local_plain_text[0], final_size);
    SHA1_Final(md_value, &sha_context);

    const unsigned char* md_ptr =
        static_cast<const unsigned char*>(local_plain_text.const_data());
    md_ptr += final_size;
    if (chromeos::SafeMemcmp(md_ptr, md_value, SHA_DIGEST_LENGTH)) {
      LOG(ERROR) << "Digest verification failed.";
      EVP_CIPHER_CTX_cleanup(&decryption_context);
      return false;
    }
  }

  local_plain_text.resize(final_size);
  plain_text->swap(local_plain_text);
  EVP_CIPHER_CTX_cleanup(&decryption_context);
  return true;
}

// AesEncryptSpecifyBlockMode encrypts the bytes in plain_text using AES,
// placing the output into encrypted.  Aside from range constraints (start and
// count) and the key and initialization vector, this method has two parameters
// that control how the ciphertext is generated and are useful in encrypting
// specific types of data in cryptohome.
//
// First, padding specifies whether and how the plaintext is padded before
// encryption.  The three options, described in the PaddingScheme enumeration
// are used as such:
//   - kPaddingNone is used to mix the user's passkey (derived from the
//     password) into the encrypted blob storing the vault keyset when the TPM
//     is used.  This is described in more detail in the README file.  There is
//     no padding in this case, and the size of plain_text needs to be a
//     multiple of the AES block size (16 bytes)
//   - kPaddingCryptohomeDefault appends a SHA1 hash of the plaintext in
//     plain_text before passing it to OpenSSL, which still uses PKCS#5 padding
//     so that we do not have to re-implement block-multiple padding ourselves.
//     This padding scheme allows us to strongly verify the plaintext on
//     decryption, which is essential when, for example, test decrypting a nonce
//     to test whether a password was correct (we do this in user_session.cc).
//
// The block mode switches between ECB and CBC.  Generally, CBC is used for most
// AES crypto that we perform, since it is a better mode for us for data that is
// larger than the block size.  We use ECB only when mixing the user passkey
// into the TPM-encrypted blob, since we only encrypt a single block of that
// data.
bool CryptoLib::AesEncryptSpecifyBlockMode(const chromeos::Blob& plain_text,
                                           unsigned int start,
                                           unsigned int count,
                                           const SecureBlob& key,
                                           const SecureBlob& iv,
                                           PaddingScheme padding,
                                           BlockMode block_mode,
                                           SecureBlob* encrypted) {
  // Verify that the range is within the data passed
  if ((start > plain_text.size()) ||
      ((start + count) > plain_text.size()) ||
      ((start + count) < start)) {
    return false;
  }
  if (count > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    // EVP_EncryptUpdate takes a signed int
    return false;
  }

  // First set the output size based on the padding scheme.  No padding means
  // that the input needs to be a multiple of the block size, and the output
  // size is equal to the input size.  Library default means we should allocate
  // up to a full block additional for the PKCS#5 padding.  Cryptohome default
  // means we should allocate a full block additional for the PKCS#5 padding and
  // enough for a SHA1 hash.
  unsigned int block_size = GetAesBlockSize();
  unsigned int needed_size = count;
  switch (padding) {
    case kPaddingCryptohomeDefault:
      // The AES block size and SHA digest length are not enough for this to
      // overflow, as needed_size is initialized to count, which must be <=
      // INT_MAX, but needed_size is itself an unsigned.  The block size and
      // digest length are fixed by the algorithm.
      needed_size += block_size + SHA_DIGEST_LENGTH;
      break;
    case kPaddingNone:
      if (count % block_size) {
        LOG(ERROR) << "Data size (" << count << ") was not a multiple "
                   << "of the block size (" << block_size << ")";
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Invalid padding specified";
      return false;
      break;
  }
  SecureBlob cipher_text(needed_size);

  // Set the block mode
  const EVP_CIPHER* cipher;
  switch (block_mode) {
    case kCbc:
      cipher = EVP_aes_256_cbc();
      break;
    case kEcb:
      cipher = EVP_aes_256_ecb();
      break;
    default:
      LOG(ERROR) << "Invalid block mode specified";
      return false;
  }
  if (key.size() != static_cast<unsigned int>(EVP_CIPHER_key_length(cipher))) {
    LOG(ERROR) << "Invalid key length of " << key.size()
               << ", expected " << EVP_CIPHER_key_length(cipher);
    return false;
  }

  // ECB ignores the IV, so only check the IV length if we are using a different
  // block mode.
  if ((block_mode != kEcb) &&
      (iv.size() != static_cast<unsigned int>(EVP_CIPHER_iv_length(cipher)))) {
    LOG(ERROR) << "Invalid iv length of " << iv.size()
               << ", expected " << EVP_CIPHER_iv_length(cipher);
    return false;
  }

  // Initialize the OpenSSL crypto context
  EVP_CIPHER_CTX encryption_context;
  EVP_CIPHER_CTX_init(&encryption_context);
  EVP_EncryptInit_ex(&encryption_context, cipher, NULL,
                     static_cast<const unsigned char*>(key.const_data()),
                     static_cast<const unsigned char*>(iv.const_data()));
  if (padding == kPaddingNone) {
    EVP_CIPHER_CTX_set_padding(&encryption_context, 0);
  }

  // First, encrypt the plain_text data
  unsigned int current_size = 0;
  int encrypt_size = 0;
  if (!EVP_EncryptUpdate(&encryption_context, &cipher_text[current_size],
                         &encrypt_size, &plain_text[start], count)) {
    LOG(ERROR) << "EncryptUpdate failed";
    EVP_CIPHER_CTX_cleanup(&encryption_context);
    return false;
  }
  current_size += encrypt_size;
  encrypt_size = 0;

  // Next, if the padding uses the cryptohome default scheme, encrypt a SHA1
  // hash of the preceding plain_text into the output data
  if (padding == kPaddingCryptohomeDefault) {
    SHA_CTX sha_context;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&sha_context);
    SHA1_Update(&sha_context, &plain_text[start], count);
    SHA1_Final(md_value, &sha_context);
    if (!EVP_EncryptUpdate(&encryption_context, &cipher_text[current_size],
                           &encrypt_size, md_value, sizeof(md_value))) {
      LOG(ERROR) << "EncryptUpdate failed";
      EVP_CIPHER_CTX_cleanup(&encryption_context);
      return false;
    }
    current_size += encrypt_size;
    encrypt_size = 0;
  }

  // Finally, finish the encryption
  if (!EVP_EncryptFinal_ex(&encryption_context, &cipher_text[current_size],
                           &encrypt_size)) {
    LOG(ERROR) << "EncryptFinal failed";
    EVP_CIPHER_CTX_cleanup(&encryption_context);
    return false;
  }
  current_size += encrypt_size;
  cipher_text.resize(current_size);

  encrypted->swap(cipher_text);
  EVP_CIPHER_CTX_cleanup(&encryption_context);
  return true;
}

// TODO(ellyjones): replace this with base::HexEncode
void CryptoLib::AsciiEncodeToBuffer(const chromeos::Blob& blob, char* buffer,
                                    unsigned int buffer_length) {
  const char hex_chars[] = "0123456789abcdef";
  unsigned int i = 0;
  for (chromeos::Blob::const_iterator it = blob.begin();
      it < blob.end() && (i + 1) < buffer_length; ++it) {
    buffer[i++] = hex_chars[((*it) >> 4) & 0x0f];
    buffer[i++] = hex_chars[(*it) & 0x0f];
  }
  if (i < buffer_length) {
    buffer[i] = '\0';
  }
}

}  // namespace cryptohome
