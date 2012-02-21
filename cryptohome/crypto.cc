// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Crypto

#include "crypto.h"

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
#include <scrypt/scryptenc.h>
}

#include "cryptohome_common.h"
#include "platform.h"
#include "username_passkey.h"

// Included last because it has conflicting defines
extern "C" {
#include <ecryptfs.h>
}

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
const int64 Crypto::kSaltMax = (1 << 20);  // 1 MB

Crypto::Crypto()
    : use_tpm_(false),
      load_tpm_(true),
      tpm_(NULL),
      fallback_to_scrypt_(true) {
}

Crypto::~Crypto() {
}

bool Crypto::Init() {
  if ((use_tpm_ || load_tpm_) && tpm_ == NULL) {
    tpm_ = Tpm::GetSingleton();
  }
  if (tpm_) {
    tpm_->Init(this, true);
  }
  return true;
}

Crypto::CryptoError Crypto::EnsureTpm(bool disconnect_first) const {
  Crypto::CryptoError result = Crypto::CE_NONE;
  if (tpm_) {
    if (disconnect_first) {
      tpm_->Disconnect();
    }
    if (!tpm_->IsConnected()) {
      Tpm::TpmRetryAction retry_action;
      if (!tpm_->Connect(&retry_action)) {
        result = TpmErrorToCrypto(retry_action);
      }
    }
  }
  return result;
}

void Crypto::GetSecureRandom(unsigned char* rand, unsigned int length) const {
  // OpenSSL takes a signed integer.  On the off chance that the user requests
  // something too large, truncate it.
  if (length > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    length = std::numeric_limits<int>::max();
  }
  RAND_bytes(rand, length);
}

unsigned int Crypto::GetAesBlockSize() const {
  return EVP_CIPHER_block_size(EVP_aes_256_cbc());
}

bool Crypto::AesEncrypt(const chromeos::Blob& plain_text, unsigned int start,
                        unsigned int count, const SecureBlob& key,
                        const SecureBlob& iv, PaddingScheme padding,
                        SecureBlob* encrypted) const {
  return AesEncryptSpecifyBlockMode(plain_text, start, count, key, iv, padding,
                                    kCbc, encrypted);
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
//   - kPaddingLibraryDefault merely defers padding to OpenSSL's implementation,
//     which is PKCS#5.  PKCS#5 padding pads the plaintext to the AES block
//     size, extending an etire block if the plaintext is already a multiple of
//     the block size.  The padding is such that the decryption will weakly
//     verify the contents, though as stated in the OpenSSL documents, this has
//     a greater than 1/256 chance of random data passing the verification.  It
//     exists as an option merely for migrating older keysets, which used PKCS#5
//     padding.
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
bool Crypto::AesEncryptSpecifyBlockMode(const chromeos::Blob& plain_text,
                                        unsigned int start, unsigned int count,
                                        const SecureBlob& key,
                                        const SecureBlob& iv,
                                        PaddingScheme padding,
                                        BlockMode block_mode,
                                        SecureBlob* encrypted) const {
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
    case kPaddingLibraryDefault:
      // The AES block size is not enough for this to overflow, as needed_size
      // is initialized to count, which must be <= INT_MAX, but needed_size is
      // itself an unsigned.  The block size is fixed by the algorithm.
      needed_size += block_size;
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

void Crypto::GetSha1(const chromeos::Blob& data, unsigned int start,
                     unsigned int count, SecureBlob* hash) const {
  if (start > data.size() ||
      ((start + count) > data.size()) ||
      ((start + count) < start)) {
    return;
  }
  SHA_CTX sha_context;
  unsigned char md_value[SHA_DIGEST_LENGTH];

  SHA1_Init(&sha_context);
  SHA1_Update(&sha_context, &data[start], count);
  SHA1_Final(md_value, &sha_context);
  hash->resize(sizeof(md_value));
  memcpy(hash->data(), md_value, sizeof(md_value));
  // Zero the stack to match expectations set by SecureBlob.
  chromeos::SecureMemset(md_value, 0, sizeof(md_value));
}

void Crypto::GetSha256(const chromeos::Blob& data, unsigned int start,
                       unsigned int count, SecureBlob* hash) const {
  if (start > data.size() ||
      ((start + count) > data.size()) ||
      ((start + count) < start)) {
    return;
  }
  SHA256_CTX sha_context;
  unsigned char md_value[SHA256_DIGEST_LENGTH];

  SHA256_Init(&sha_context);
  SHA256_Update(&sha_context, &data[start], count);
  SHA256_Final(md_value, &sha_context);
  hash->resize(sizeof(md_value));
  memcpy(hash->data(), md_value, sizeof(md_value));
  // Zero the stack to match expectations set by SecureBlob.
  chromeos::SecureMemset(md_value, 0, sizeof(md_value));
}

bool Crypto::AesDecrypt(const chromeos::Blob& encrypted, unsigned int start,
                        unsigned int count, const SecureBlob& key,
                        const SecureBlob& iv, PaddingScheme padding,
                        SecureBlob* plain_text) const {
  return AesDecryptSpecifyBlockMode(encrypted, start, count, key, iv, padding,
                                    kCbc, plain_text);
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
bool Crypto::AesDecryptSpecifyBlockMode(const chromeos::Blob& encrypted,
                                        unsigned int start, unsigned int count,
                                        const SecureBlob& key,
                                        const SecureBlob& iv,
                                        PaddingScheme padding,
                                        BlockMode block_mode,
                                        SecureBlob* plain_text) const {
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

bool Crypto::PasskeyToAesKey(const chromeos::Blob& passkey,
                             const chromeos::Blob& salt, unsigned int rounds,
                             SecureBlob* key, SecureBlob* iv) const {
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

bool Crypto::CreateRsaKey(unsigned int key_bits,
                          SecureBlob* n,
                          SecureBlob* p) const {
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

void Crypto::PasskeyToKeysetKey(const chromeos::Blob& passkey,
                                const chromeos::Blob& salt, unsigned int iters,
                                SecureBlob* key) const {
  unsigned int update_length = passkey.size();
  unsigned int holder_size = SHA_DIGEST_LENGTH;
  if (update_length > SHA_DIGEST_LENGTH) {
    holder_size = update_length;
  }
  SecureBlob holder(holder_size);
  memcpy(&holder[0], &passkey[0], update_length);

  // Repeatedly hash the user passkey and salt to generate the key
  for (unsigned int i = 0; i < iters; ++i) {
    SHA_CTX sha_context;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&sha_context);
    SHA1_Update(&sha_context, &salt[0], salt.size());
    SHA1_Update(&sha_context, &holder[0], update_length);
    SHA1_Final(md_value, &sha_context);

    memcpy(&holder[0], md_value, SHA_DIGEST_LENGTH);
    update_length = SHA_DIGEST_LENGTH;
  }

  holder.resize(update_length);
  SecureBlob local_key(update_length * 2);
  AsciiEncodeToBuffer(holder, static_cast<char*>(local_key.data()),
                      local_key.size());
  key->swap(local_key);
}

bool Crypto::GetOrCreateSalt(const FilePath& path, unsigned int length,
                             bool force, SecureBlob* salt) const {
  int64 file_len = 0;
  if (file_util::PathExists(path)) {
    if (!file_util::GetFileSize(path, &file_len)) {
      LOG(ERROR) << "Can't get file len for " << path.value();
      return false;
    }
  }

  SecureBlob local_salt;
  if (force || file_len == 0 || file_len > kSaltMax) {
    LOG(ERROR) << "Creating new salt";
    // If this salt doesn't exist, automatically create it
    local_salt.resize(length);
    GetSecureRandom(static_cast<unsigned char*>(local_salt.data()),
                    local_salt.size());
    unsigned int data_written = file_util::WriteFile(path,
        static_cast<const char*>(local_salt.const_data()),
        length);
    if (data_written != length) {
      LOG(ERROR) << "Could not write user salt";
      return false;
    }
    sync();
  } else {
    local_salt.resize(file_len);
    int data_read = file_util::ReadFile(path,
                                        static_cast<char*>(local_salt.data()),
                                        local_salt.size());
    if (data_read <= 0 || static_cast<int64>(data_read) != file_len) {
      LOG(ERROR) << "Could not read entire file " << file_len;
      return false;
    }
  }
  salt->swap(local_salt);
  return true;
}

bool Crypto::AddKeyset(const VaultKeyset& vault_keyset,
                       std::string* key_signature,
                       std::string* filename_key_signature) const {
  // Add the File Encryption key (FEK) from the vault keyset.  This is the key
  // that is used to encrypt the file contents when it is persisted to the lower
  // filesystem by eCryptfs.
  *key_signature = chromeos::AsciiEncode(vault_keyset.FEK_SIG());
  if (!PushVaultKey(vault_keyset.FEK(), *key_signature,
                    vault_keyset.FEK_SALT())) {
    LOG(ERROR) << "Couldn't add ecryptfs key to keyring";
    return false;
  }

  // Add the File Name Encryption Key (FNEK) from the vault keyset.  This is the
  // key that is used to encrypt the file name when it is persisted to the lower
  // filesystem by eCryptfs.
  *filename_key_signature = chromeos::AsciiEncode(vault_keyset.FNEK_SIG());
  if (!PushVaultKey(vault_keyset.FNEK(), *filename_key_signature,
                    vault_keyset.FNEK_SALT())) {
    LOG(ERROR) << "Couldn't add ecryptfs filename encryption key to keyring";
    return false;
  }

  return true;
}

void Crypto::ClearKeyset() const {
  Platform::ClearUserKeyring();
}

Crypto::CryptoError Crypto::TpmErrorToCrypto(
    Tpm::TpmRetryAction retry_action) const {
  switch (retry_action) {
    case Tpm::Fatal:
      return Crypto::CE_TPM_FATAL;
    case Tpm::RetryCommFailure:
      return Crypto::CE_TPM_COMM_ERROR;
    case Tpm::RetryDefendLock:
      return Crypto::CE_TPM_DEFEND_LOCK;
    default:
      return Crypto::CE_NONE;
  }
}

bool Crypto::PushVaultKey(const SecureBlob& key, const std::string& key_sig,
                          const SecureBlob& salt) const {
  DCHECK(key.size() == ECRYPTFS_MAX_KEY_BYTES);
  DCHECK(key_sig.length() == (ECRYPTFS_SIG_SIZE * 2));
  DCHECK(salt.size() == ECRYPTFS_SALT_SIZE);

  struct ecryptfs_auth_tok auth_token;

  generate_payload(&auth_token, const_cast<char*>(key_sig.c_str()),
                   const_cast<char*>(reinterpret_cast<const char*>(&salt[0])),
                   const_cast<char*>(reinterpret_cast<const char*>(&key[0])));

  if (ecryptfs_add_auth_tok_to_keyring(&auth_token,
          const_cast<char*>(key_sig.c_str())) < 0) {
    LOG(ERROR) << "PushVaultKey failed";
  }

  return true;
}

void Crypto::PasswordToPasskey(const char* password,
                               const chromeos::Blob& salt,
                               SecureBlob* passkey) {
  CHECK(password);

  std::string ascii_salt = chromeos::AsciiEncode(salt);
  // Convert a raw password to a password hash
  SHA256_CTX sha_context;
  SecureBlob md_value(SHA256_DIGEST_LENGTH);

  SHA256_Init(&sha_context);
  SHA256_Update(&sha_context,
                reinterpret_cast<const unsigned char*>(ascii_salt.data()),
                static_cast<unsigned int>(ascii_salt.length()));
  SHA256_Update(&sha_context, password, strlen(password));
  SHA256_Final(static_cast<unsigned char*>(md_value.data()), &sha_context);

  md_value.resize(SHA256_DIGEST_LENGTH / 2);
  SecureBlob local_passkey(SHA256_DIGEST_LENGTH);
  AsciiEncodeToBuffer(md_value, static_cast<char*>(local_passkey.data()),
                      local_passkey.size());
  passkey->swap(local_passkey);
}

void Crypto::AsciiEncodeToBuffer(const chromeos::Blob& blob, char* buffer,
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

bool Crypto::DecryptVaultKeyset(const SerializedVaultKeyset& serialized,
                                const chromeos::Blob& vault_key,
                                unsigned int* crypt_flags, CryptoError* error,
                                VaultKeyset* vault_keyset) const {
  if (crypt_flags) {
    *crypt_flags = 0;
  }
  if (error) {
    *error = CE_NONE;
  }
  SecureBlob local_encrypted_keyset(serialized.wrapped_keyset().length());
  serialized.wrapped_keyset().copy(
      static_cast<char*>(local_encrypted_keyset.data()),
      serialized.wrapped_keyset().length(), 0);
  SecureBlob salt(serialized.salt().length());
  serialized.salt().copy(static_cast<char*>(salt.data()),
                         serialized.salt().length(), 0);

  // On decrypt, default to the legacy password rounds, and use the value in the
  // SerializedVaultKeyset if it exists
  unsigned int rounds;
  if (serialized.has_password_rounds()) {
    rounds = serialized.password_rounds();
  } else {
    rounds = kDefaultLegacyPasswordRounds;
  }

  SecureBlob local_vault_key(vault_key.begin(), vault_key.end());
  // Check if the vault keyset was TPM-wrapped
  if ((serialized.flags() & SerializedVaultKeyset::TPM_WRAPPED)
      && serialized.has_tpm_key()) {
    if (crypt_flags) {
      *crypt_flags |= SerializedVaultKeyset::TPM_WRAPPED;
    }
    // If the TPM is enabled but not owned, and the keyset is TPM wrapped, then
    // it means the TPM has been cleared since the last login, and is not
    // re-owned.  In this case, the SRK is cleared and we cannot recover the
    // keyset.
    if (tpm_->IsEnabled() && !tpm_->IsOwned()) {
      LOG(ERROR) << "Fatal error--the TPM is enabled but not owned, and this "
                 << "keyset was wrapped by the TPM.  It is impossible to "
                 << "recover this keyset.";
      if (error) {
        *error = CE_TPM_FATAL;
      }
      return false;
    }
    Crypto::CryptoError local_error = EnsureTpm(false);
    if (!tpm_->IsConnected()) {
      LOG(ERROR) << "Vault keyset is wrapped by the TPM, but the TPM is "
                 << "unavailable";
      if (error) {
        *error = local_error;
      }
      return false;
    }
    // Check if the public key for this keyset matches the public key on this
    // TPM.  If not, we cannot recover.
    Tpm::TpmRetryAction retry_action;
    if (serialized.has_tpm_public_key_hash()) {
      SecureBlob serialized_pub_key_hash(
          serialized.tpm_public_key_hash().length());
      serialized.tpm_public_key_hash().copy(
          static_cast<char*>(serialized_pub_key_hash.data()),
          serialized.tpm_public_key_hash().length(),
          0);
      SecureBlob pub_key;
      if (!tpm_->GetPublicKey(&pub_key, &retry_action)) {
        LOG(ERROR) << "Unable to get the cryptohome public key from the TPM.";
        if (error) {
          // This is a fatal error only if we don't get a transient error code.
          *error = TpmErrorToCrypto(retry_action);
        }
        return false;
      }
      SecureBlob pub_key_hash;
      GetSha1(pub_key, 0, pub_key.size(), &pub_key_hash);
      if ((serialized_pub_key_hash.size() != pub_key_hash.size()) ||
          (chromeos::SafeMemcmp(serialized_pub_key_hash.data(),
                  pub_key_hash.data(),
                  pub_key_hash.size()))) {
        LOG(ERROR) << "Fatal key error--the cryptohome public key does not "
                   << "match the one used to encrypt this keyset.";
        if (error) {
          *error = CE_TPM_FATAL;
        }
        return false;
      }
    }
    SecureBlob tpm_key(serialized.tpm_key().length());
    serialized.tpm_key().copy(static_cast<char*>(tpm_key.data()),
                              serialized.tpm_key().length(), 0);
    if (!tpm_->Decrypt(tpm_key, vault_key, rounds, salt,
                       &local_vault_key, &retry_action)) {
      LOG(ERROR) << "The TPM failed to unwrap the intermediate key with the "
                 << "supplied credentials";
      if (error) {
        // This is a fatal error only if we don't get a transient error code.
        *error = TpmErrorToCrypto(retry_action);
      }
      return false;
    }
  }

  SecureBlob plain_text;
  if (serialized.flags() & SerializedVaultKeyset::SCRYPT_WRAPPED) {
    if (crypt_flags) {
      *crypt_flags |= SerializedVaultKeyset::SCRYPT_WRAPPED;
    }
    size_t out_len = serialized.wrapped_keyset().length();
    SecureBlob decrypted(out_len);
    int scrypt_rc;
    if ((scrypt_rc = scryptdec_buf(
            static_cast<const uint8_t*>(local_encrypted_keyset.const_data()),
            local_encrypted_keyset.size(),
            static_cast<uint8_t*>(decrypted.data()),
            &out_len,
            static_cast<const uint8_t*>(&vault_key[0]),
            vault_key.size(),
            kScryptMaxMem,
            100.0,
            kScryptMaxDecryptTime))) {
      LOG(ERROR) << "Scrypt decryption failed: " << scrypt_rc;
      if (error) {
        *error = CE_SCRYPT_CRYPTO;
      }
      return false;
    }
    decrypted.resize(out_len);
    SecureBlob hash;
    unsigned int hash_offset = decrypted.size() - SHA_DIGEST_LENGTH;
    GetSha1(decrypted, 0, hash_offset, &hash);
    if (chromeos::SafeMemcmp(hash.data(), &decrypted[hash_offset],
                             hash.size())) {
      LOG(ERROR) << "Scrypt hash verification failed";
      if (error) {
        *error = CE_SCRYPT_CRYPTO;
      }
      return false;
    }
    decrypted.resize(hash_offset);
    plain_text.swap(decrypted);
  } else {
    SecureBlob aes_key;
    SecureBlob iv;
    if (!PasskeyToAesKey(local_vault_key,
                         salt,
                         rounds,
                         &aes_key,
                         &iv)) {
      LOG(ERROR) << "Failure converting passkey to key";
      if (error) {
        *error = CE_OTHER_FATAL;
      }
      return false;
    }

    if (!AesDecrypt(local_encrypted_keyset, 0, local_encrypted_keyset.size(),
                    aes_key, iv, kPaddingCryptohomeDefault, &plain_text)) {
      LOG(ERROR) << "AES decryption failed.";
      if (error) {
        *error = CE_OTHER_CRYPTO;
      }
      return false;
    }
  }

  vault_keyset->FromKeysBlob(plain_text);
  // Check if the public key hash was stored
  if (!serialized.has_tpm_public_key_hash() && serialized.has_tpm_key()) {
    if (error) {
      *error = CE_NO_PUBLIC_KEY_HASH;
    }
  }
  return true;
}

bool Crypto::EncryptVaultKeyset(const VaultKeyset& vault_keyset,
                                const SecureBlob& vault_key,
                                const SecureBlob& vault_key_salt,
                                SerializedVaultKeyset* serialized) const {
  SecureBlob keyset_blob;
  if (!vault_keyset.ToKeysBlob(&keyset_blob)) {
    LOG(ERROR) << "Failure serializing keyset to buffer";
    return false;
  }

  unsigned int rounds = kDefaultPasswordRounds;
  bool tpm_wrapped = false;
  // The local_vault_key is used as the AES key to encrypt the user's vault
  // keyset (vault keyset key, or VKK).  We initialize it to the passed-in
  // vault_key, which is the user's passkey.
  SecureBlob local_vault_key(vault_key.begin(), vault_key.end());
  SecureBlob tpm_key;
  // Check if the vault keyset should be TPM-wrapped
  if (use_tpm_) {
    // Ignore the status here.  When the TPM is not available, we merely fall
    // back to scrypt.
    EnsureTpm(false);
  }
  // If the TPM is available, then instead of using the user's passkey as the
  // local_vault_key, we generate a random string of bytes to use as the VKK
  // instead.  In this case, the user's passkey is used with the TPM to store
  // the encrypted VKK in the serialized keyset.  To decrypt the vault keyset,
  // the VKK must be decrypted using the TPM+user passkey, and then the VKK is
  // used to decrypt the vault keyset. For now, we allow fall-through to
  // non-TPM-wrapped if tpm_ is NULL.  When this happens, it uses Scrypt to
  // protect the vault keyset.
  if (use_tpm_ && tpm_ != NULL && tpm_->IsConnected()) {
    // If we are using the TPM, the local_vault_key becomes a
    // randomly-generated AES key.  It is this random key that actually encrypts
    // the vault keyset, and itself is wrapped by the TPM.
    local_vault_key.resize(kDefaultAesKeySize);
    GetSecureRandom(static_cast<unsigned char*>(local_vault_key.data()),
                    local_vault_key.size());
    Tpm::TpmRetryAction retry_action;
    // Encrypt the VKK using the TPM and the user's passkey.  The output is an
    // encrypted blob in tpm_key, which is stored in the serialized vault
    // keyset.
    if (tpm_->Encrypt(local_vault_key, vault_key, rounds, vault_key_salt,
                      &tpm_key, &retry_action)) {
      tpm_wrapped = true;
    } else {
      // Re-place the user's passkey into the VKK.  This allows fallback to
      // Scrypt-based protection when the TPM didn't work for any reason.
      local_vault_key.resize(vault_key.size());
      memcpy(local_vault_key.data(), vault_key.const_data(),
             vault_key.size());
      LOG(ERROR) << "The TPM failed to wrap the intermediate key with the "
                 << "supplied credentials.  The vault keyset will not be "
                 << "further secured by the TPM.";
    }
  }

  SecureBlob cipher_text;
  bool scrypt_wrapped = false;
  // If scrypt is enabled (it is by default, but it can be disabled for unit
  // tests), and the TPM wasn't used, then we use scrypt to encrypt the keyset.
  // Otherwise, we use AES with whatever the VKK is.  When the TPM is available,
  // the VKK is a random key (see above).
  if (fallback_to_scrypt_ && !tpm_wrapped) {
    // Append the SHA1 hash of the keyset blob
    SecureBlob hash;
    GetSha1(keyset_blob, 0, keyset_blob.size(), &hash);
    SecureBlob local_keyset_blob(keyset_blob.size() + hash.size());
    memcpy(&local_keyset_blob[0], keyset_blob.const_data(), keyset_blob.size());
    memcpy(&local_keyset_blob[keyset_blob.size()], hash.data(), hash.size());
    cipher_text.resize(local_keyset_blob.size() + kScryptHeaderLength);
    int scrypt_rc;
    if ((scrypt_rc = scryptenc_buf(
            static_cast<const uint8_t*>(local_keyset_blob.const_data()),
            local_keyset_blob.size(),
            static_cast<uint8_t*>(cipher_text.data()),
            static_cast<const uint8_t*>(&vault_key[0]),
            vault_key.size(),
            kScryptMaxMem,
            100.0,
            kScryptMaxEncryptTime))) {
      LOG(ERROR) << "Scrypt encryption failed: " << scrypt_rc;
      return false;
    }
    scrypt_wrapped = true;
  } else {
    SecureBlob aes_key;
    SecureBlob iv;
    if (!PasskeyToAesKey(local_vault_key, vault_key_salt,
                         rounds, &aes_key, &iv)) {
      LOG(ERROR) << "Failure converting passkey to key";
      return false;
    }

    if (!AesEncrypt(keyset_blob, 0, keyset_blob.size(), aes_key, iv,
                    kPaddingCryptohomeDefault, &cipher_text)) {
      LOG(ERROR) << "AES encryption failed.";
      return false;
    }
  }

  unsigned int keyset_flags = SerializedVaultKeyset::NONE;
  if (tpm_wrapped) {
    // Store the TPM-encrypted key
    keyset_flags = SerializedVaultKeyset::TPM_WRAPPED;
    serialized->set_tpm_key(tpm_key.const_data(),
                            tpm_key.size());
    // Store the hash of the cryptohome public key
    SecureBlob pub_key;
    Tpm::TpmRetryAction retry_action;
    // Allow this to fail.  It is not absolutely necessary; it allows us to
    // detect a TPM clear.  If this fails due to a transient issue, then on next
    // successful login, the vault keyset will be re-saved anyway.
    if (tpm_->GetPublicKey(&pub_key, &retry_action)) {
      SecureBlob pub_key_hash;
      GetSha1(pub_key, 0, pub_key.size(), &pub_key_hash);
      serialized->set_tpm_public_key_hash(pub_key_hash.const_data(),
                                         pub_key_hash.size());
    }
  }
  if (scrypt_wrapped) {
    keyset_flags |= SerializedVaultKeyset::SCRYPT_WRAPPED;
  }
  serialized->set_flags(keyset_flags);
  serialized->set_salt(vault_key_salt.const_data(),
                       vault_key_salt.size());
  serialized->set_wrapped_keyset(cipher_text.const_data(), cipher_text.size());
  serialized->set_password_rounds(rounds);
  return true;
}

}  // namespace cryptohome
