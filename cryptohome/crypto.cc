// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Crypto

#include "crypto.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/utility.h>
extern "C" {
#include <scrypt/scryptenc.h>
}

#include "cryptohome_common.h"
#include "old_vault_keyset.h"
#include "platform.h"
#include "username_passkey.h"
#include "vault_keyset.pb.h"

// Included last because it has conflicting defines
extern "C" {
#include <ecryptfs.h>
}

using std::string;

namespace cryptohome {

const std::string kDefaultEntropySource = "/dev/urandom";
const std::string kOpenSSLMagic = "Salted__";
const int kWellKnownExponent = 65537;
const int kScryptMaxMem = 32 * 1024 * 1024;
const double kScryptMaxEncryptTime = 0.333;
const double kScryptMaxDecryptTime = 1.0;
const int kScryptHeaderLength = 128;
const int kDefaultLegacyPasswordRounds = 1;
const int kDefaultPasswordRounds = 1337;

Crypto::Crypto()
    : entropy_source_(kDefaultEntropySource),
      use_tpm_(false),
      load_tpm_(true),
      default_tpm_(new Tpm()),
      tpm_(NULL),
      fallback_to_scrypt_(false) {
}

Crypto::~Crypto() {
}

bool Crypto::Init() {
  SeedRng();
  if ((use_tpm_ || load_tpm_) && tpm_ == NULL) {
    tpm_ = default_tpm_.get();
  }
  if (tpm_) {
    if (!tpm_->Init(this, true)) {
      tpm_ = NULL;
    }
  }
  return true;
}

void Crypto::EnsureTpm() const {
  if (tpm_ == NULL) {
    const_cast<Crypto*>(this)->tpm_ = default_tpm_.get();
  }
  if (tpm_) {
    if (!tpm_->IsConnected()) {
      if (!tpm_->Connect()) {
        const_cast<Crypto*>(this)->tpm_ = NULL;
      }
    }
  }
}

void Crypto::SeedRng() const {
  // TODO(fes): Get assistance from the TPM when it is available
  while (!RAND_status()) {
    char buffer[256];
    file_util::ReadFile(FilePath(entropy_source_), buffer, sizeof(buffer));
    RAND_add(buffer, sizeof(buffer), sizeof(buffer));
  }
}

void Crypto::GetSecureRandom(unsigned char *rand, int length) const {
  SeedRng();
  // Have OpenSSL generate the random bytes
  RAND_bytes(rand, length);
}

unsigned int Crypto::GetAesBlockSize() const {
  return EVP_CIPHER_block_size(EVP_aes_256_cbc());
}

bool Crypto::WrapAes(const chromeos::Blob& unwrapped, unsigned int start,
                     unsigned int count, const SecureBlob& key,
                     const SecureBlob& iv, PaddingScheme padding,
                     SecureBlob* wrapped) const {
  return WrapAesSpecifyBlockMode(unwrapped, start, count, key, iv, padding,
                                 CBC, wrapped);
}

bool Crypto::WrapAesSpecifyBlockMode(const chromeos::Blob& unwrapped,
                                     unsigned int start, unsigned int count,
                                     const SecureBlob& key,
                                     const SecureBlob& iv,
                                     PaddingScheme padding,
                                     BlockMode block_mode,
                                     SecureBlob* wrapped) const {
  if ((start + count) > unwrapped.size()) {
    return false;
  }
  int block_size = GetAesBlockSize();
  int needed_size = count;
  switch (padding) {
    case PADDING_CRYPTOHOME_DEFAULT:
      needed_size += block_size + SHA_DIGEST_LENGTH;
      break;
    case PADDING_LIBRARY_DEFAULT:
      needed_size += block_size;
      break;
    case PADDING_NONE:
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

  const EVP_CIPHER *cipher;
  switch (block_mode) {
    case CBC:
      cipher = EVP_aes_256_cbc();
      break;
    case ECB:
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
  if ((block_mode != ECB) &&
      (iv.size() != static_cast<unsigned int>(EVP_CIPHER_iv_length(cipher)))) {
    LOG(ERROR) << "Invalid iv length of " << iv.size()
               << ", expected " << EVP_CIPHER_iv_length(cipher);
    return false;
  }
  EVP_CIPHER_CTX e_ctx;
  EVP_CIPHER_CTX_init(&e_ctx);
  EVP_EncryptInit_ex(&e_ctx, cipher, NULL,
                     static_cast<const unsigned char*>(key.const_data()),
                     static_cast<const unsigned char*>(iv.const_data()));
  if (padding == PADDING_NONE) {
    EVP_CIPHER_CTX_set_padding(&e_ctx, 0);
  }
  int current_size = 0;
  int encrypt_size = 0;
  if (!EVP_EncryptUpdate(&e_ctx, &cipher_text[current_size], &encrypt_size,
                         &unwrapped[start], count)) {
    LOG(ERROR) << "EncryptUpdate failed";
    EVP_CIPHER_CTX_cleanup(&e_ctx);
    return false;
  }
  current_size += encrypt_size;
  encrypt_size = 0;

  if (padding == PADDING_CRYPTOHOME_DEFAULT) {
    SHA_CTX sha_ctx;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&sha_ctx);
    SHA1_Update(&sha_ctx, &unwrapped[start], count);
    SHA1_Final(md_value, &sha_ctx);
    if (!EVP_EncryptUpdate(&e_ctx, &cipher_text[current_size], &encrypt_size,
                           md_value, sizeof(md_value))) {
      LOG(ERROR) << "EncryptUpdate failed";
      EVP_CIPHER_CTX_cleanup(&e_ctx);
      return false;
    }
    current_size += encrypt_size;
    encrypt_size = 0;
  }

  // Finish the encryption
  if (!EVP_EncryptFinal_ex(&e_ctx, &cipher_text[current_size], &encrypt_size)) {
    LOG(ERROR) << "EncryptFinal failed";
    EVP_CIPHER_CTX_cleanup(&e_ctx);
    return false;
  }
  current_size += encrypt_size;
  cipher_text.resize(current_size);

  wrapped->swap(cipher_text);
  EVP_CIPHER_CTX_cleanup(&e_ctx);
  return true;
}

void Crypto::GetSha1(const chromeos::Blob& data, int start, int count,
                     SecureBlob* hash) const {
  SHA_CTX sha_ctx;
  unsigned char md_value[SHA_DIGEST_LENGTH];

  SHA1_Init(&sha_ctx);
  SHA1_Update(&sha_ctx, &data[start], count);
  SHA1_Final(md_value, &sha_ctx);
  hash->resize(sizeof(md_value));
  memcpy(hash->data(), md_value, sizeof(md_value));
}

bool Crypto::UnwrapAes(const chromeos::Blob& wrapped, unsigned int start,
                       unsigned int count, const SecureBlob& key,
                       const SecureBlob& iv, PaddingScheme padding,
                       SecureBlob* unwrapped) const {
  return UnwrapAesSpecifyBlockMode(wrapped, start, count, key, iv, padding,
                                   CBC, unwrapped);
}

bool Crypto::UnwrapAesSpecifyBlockMode(const chromeos::Blob& wrapped,
                                       unsigned int start, unsigned int count,
                                       const SecureBlob& key,
                                       const SecureBlob& iv,
                                       PaddingScheme padding,
                                       BlockMode block_mode,
                                       SecureBlob* unwrapped) const {
  if ((start + count) > wrapped.size()) {
    return false;
  }
  SecureBlob plain_text(count);

  int final_size = 0;
  int decrypt_size = plain_text.size();

  const EVP_CIPHER *cipher;
  switch (block_mode) {
    case CBC:
      cipher = EVP_aes_256_cbc();
      break;
    case ECB:
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
  if ((block_mode != ECB) &&
      (iv.size() != static_cast<unsigned int>(EVP_CIPHER_iv_length(cipher)))) {
    LOG(ERROR) << "Invalid iv length of " << iv.size()
               << ", expected " << EVP_CIPHER_iv_length(cipher);
    return false;
  }

  EVP_CIPHER_CTX d_ctx;
  EVP_CIPHER_CTX_init(&d_ctx);
  EVP_DecryptInit_ex(&d_ctx, cipher, NULL,
                     static_cast<const unsigned char*>(key.const_data()),
                     static_cast<const unsigned char*>(iv.const_data()));
  if (padding == PADDING_NONE) {
    EVP_CIPHER_CTX_set_padding(&d_ctx, 0);
  }
  if (!EVP_DecryptUpdate(&d_ctx, &plain_text[0], &decrypt_size,
                         &wrapped[start], count)) {
    LOG(ERROR) << "DecryptUpdate failed";
    EVP_CIPHER_CTX_cleanup(&d_ctx);
    return false;
  }
  if (!EVP_DecryptFinal_ex(&d_ctx, &plain_text[decrypt_size], &final_size)) {
    unsigned long err = ERR_get_error();
    ERR_load_ERR_strings();
    ERR_load_crypto_strings();

    LOG(ERROR) << "DecryptFinal Error: " << err
               << ": " << ERR_lib_error_string(err)
               << ", " << ERR_func_error_string(err)
               << ", " << ERR_reason_error_string(err);

    EVP_CIPHER_CTX_cleanup(&d_ctx);
    return false;
  }
  final_size += decrypt_size;

  if (padding == PADDING_CRYPTOHOME_DEFAULT) {
    if (final_size < SHA_DIGEST_LENGTH) {
      LOG(ERROR) << "Plain text was too small.";
      EVP_CIPHER_CTX_cleanup(&d_ctx);
      return false;
    }

    final_size -= SHA_DIGEST_LENGTH;

    SHA_CTX sha_ctx;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&sha_ctx);
    SHA1_Update(&sha_ctx, &plain_text[0], final_size);
    SHA1_Final(md_value, &sha_ctx);

    const unsigned char* md_ptr =
        static_cast<const unsigned char *>(plain_text.const_data());
    md_ptr += final_size;
    // TODO(fes): Port/use SafeMemcmp
    if (memcmp(md_ptr, md_value, SHA_DIGEST_LENGTH)) {
      LOG(ERROR) << "Digest verification failed.";
      EVP_CIPHER_CTX_cleanup(&d_ctx);
      return false;
    }
  }

  plain_text.resize(final_size);
  unwrapped->swap(plain_text);
  EVP_CIPHER_CTX_cleanup(&d_ctx);
  return true;
}

bool Crypto::PasskeyToAesKey(const chromeos::Blob& passkey,
                             const chromeos::Blob& salt, int rounds,
                             SecureBlob* key, SecureBlob* iv) const {
  if (salt.size() != PKCS5_SALT_LEN) {
    LOG(ERROR) << "Bad salt size.";
    return false;
  }

  const EVP_CIPHER *cipher = EVP_aes_256_cbc();
  SecureBlob wrapper_key(EVP_CIPHER_key_length(cipher));
  SecureBlob local_iv(EVP_CIPHER_iv_length(cipher));

  // Convert the passkey to a key
  if (!EVP_BytesToKey(cipher,
                      EVP_sha1(),
                      &salt[0],
                      &passkey[0],
                      passkey.size(),
                      rounds,
                      static_cast<unsigned char*>(wrapper_key.data()),
                      static_cast<unsigned char*>(local_iv.data()))) {
    LOG(ERROR) << "Failure converting bytes to key";
    return false;
  }

  key->swap(wrapper_key);
  if (iv) {
    iv->swap(local_iv);
  }

  return true;
}

bool Crypto::CreateRsaKey(int key_bits, SecureBlob* n, SecureBlob* p) const {
  SeedRng();

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

void Crypto::PasskeyToWrapper(const chromeos::Blob& passkey,
                              const chromeos::Blob& salt, int iters,
                              SecureBlob* wrapper) const {
  int update_length = passkey.size();
  int holder_size = SHA_DIGEST_LENGTH;
  if (update_length > SHA_DIGEST_LENGTH) {
    holder_size = update_length;
  }
  SecureBlob holder(holder_size);
  memcpy(&holder[0], &passkey[0], update_length);

  // Repeatedly hash the user passkey and salt to generate the wrapper
  for (int i = 0; i < iters; ++i) {
    SHA_CTX ctx;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&ctx);
    SHA1_Update(&ctx, &salt[0], salt.size());
    SHA1_Update(&ctx, &holder[0], update_length);
    SHA1_Final(md_value, &ctx);

    memcpy(&holder[0], md_value, SHA_DIGEST_LENGTH);
    update_length = SHA_DIGEST_LENGTH;
  }

  holder.resize(update_length);
  SecureBlob local_wrapper(update_length * 2);
  AsciiEncodeToBuffer(holder, static_cast<char*>(local_wrapper.data()),
                      local_wrapper.size());
  wrapper->swap(local_wrapper);
}

bool Crypto::GetOrCreateSalt(const FilePath& path, int length, bool force,
                             SecureBlob* salt) const {
  SecureBlob local_salt;
  if (force || !file_util::PathExists(path)) {
    // If this salt doesn't exist, automatically create it
    local_salt.resize(length);
    GetSecureRandom(static_cast<unsigned char*>(local_salt.data()),
                    local_salt.size());
    int data_written = file_util::WriteFile(path,
        static_cast<const char*>(local_salt.const_data()),
        length);
    if (data_written != length) {
      LOG(ERROR) << "Could not write user salt";
      return false;
    }
  } else {
    // Otherwise just load the contents of the salt
    int64 file_size;
    if (!file_util::GetFileSize(path, &file_size)) {
      LOG(ERROR) << "Could not get size of " << path.value();
      return false;
    }
    if (file_size > INT_MAX) {
      LOG(ERROR) << "File " << path.value() << " is too large: " << file_size;
      return false;
    }
    local_salt.resize(file_size);
    int data_read = file_util::ReadFile(path,
                                        static_cast<char*>(local_salt.data()),
                                        local_salt.size());
    if (data_read != file_size) {
      LOG(ERROR) << "Could not read entire file " << file_size;
      return false;
    }
  }
  salt->swap(local_salt);
  return true;
}

bool Crypto::AddKeyset(const VaultKeyset& vault_keyset,
                       std::string* key_signature,
                       std::string* fnek_signature) const {
  // Add the FEK
  *key_signature = chromeos::AsciiEncode(vault_keyset.FEK_SIG());
  if (!PushVaultKey(vault_keyset.FEK(), *key_signature,
                    vault_keyset.FEK_SALT())) {
    LOG(ERROR) << "Couldn't add ecryptfs key to keyring";
    return false;
  }

  // Add the FNEK
  *fnek_signature = chromeos::AsciiEncode(vault_keyset.FNEK_SIG());
  if (!PushVaultKey(vault_keyset.FNEK(), *fnek_signature,
                    vault_keyset.FNEK_SALT())) {
    LOG(ERROR) << "Couldn't add ecryptfs fnek key to keyring";
    return false;
  }

  return true;
}

void Crypto::ClearKeyset() const {
  Platform::ClearUserKeyring();
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

void Crypto::PasswordToPasskey(const char *password,
                               const chromeos::Blob& salt,
                               SecureBlob* passkey) {
  CHECK(password);

  std::string ascii_salt = chromeos::AsciiEncode(salt);
  // Convert a raw password to a password hash
  SHA256_CTX sha_ctx;
  SecureBlob md_value(SHA256_DIGEST_LENGTH);

  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx,
                reinterpret_cast<const unsigned char*>(ascii_salt.data()),
                static_cast<unsigned int>(ascii_salt.length()));
  SHA256_Update(&sha_ctx, password, strlen(password));
  SHA256_Final(static_cast<unsigned char*>(md_value.data()), &sha_ctx);

  md_value.resize(SHA256_DIGEST_LENGTH / 2);
  SecureBlob local_passkey(SHA256_DIGEST_LENGTH);
  AsciiEncodeToBuffer(md_value, static_cast<char*>(local_passkey.data()),
                      local_passkey.size());
  passkey->swap(local_passkey);
}

void Crypto::AsciiEncodeToBuffer(const chromeos::Blob& blob, char* buffer,
                                 int buffer_length) {
  const char hex_chars[] = "0123456789abcdef";
  int i = 0;
  for (chromeos::Blob::const_iterator it = blob.begin();
      it < blob.end() && (i + 1) < buffer_length; ++it) {
    buffer[i++] = hex_chars[((*it) >> 4) & 0x0f];
    buffer[i++] = hex_chars[(*it) & 0x0f];
  }
  if (i < buffer_length) {
    buffer[i] = '\0';
  }
}

bool Crypto::UnwrapVaultKeyset(const chromeos::Blob& wrapped_keyset,
                               const chromeos::Blob& vault_wrapper,
                               int* wrap_flags, CryptoError* error,
                               VaultKeyset* vault_keyset) const {
  if (wrap_flags) {
    *wrap_flags = 0;
  }
  if (error) {
    *error = CE_NONE;
  }
  SerializedVaultKeyset serialized;
  if (!serialized.ParseFromArray(
           static_cast<const unsigned char*>(&wrapped_keyset[0]),
           wrapped_keyset.size())) {
    LOG(ERROR) << "Vault keyset deserialization failed, it must be corrupt.";
    if (error) {
      *error = CE_OTHER_FATAL;
    }
    return false;
  }
  SecureBlob local_wrapped_keyset(serialized.wrapped_keyset().length());
  serialized.wrapped_keyset().copy(
      static_cast<char*>(local_wrapped_keyset.data()),
      serialized.wrapped_keyset().length(), 0);
  SecureBlob salt(serialized.salt().length());
  serialized.salt().copy(static_cast<char*>(salt.data()),
                         serialized.salt().length(), 0);

  // On unwrap, default to the legacy password rounds, and use the value in the
  // SerializedVaultKeyset if it exists
  int rounds;
  if (serialized.has_password_rounds()) {
    rounds = serialized.password_rounds();
  } else {
    rounds = kDefaultLegacyPasswordRounds;
  }

  SecureBlob local_vault_wrapper(vault_wrapper.begin(), vault_wrapper.end());
  // Check if the vault keyset was TPM-wrapped
  if ((serialized.flags() & SerializedVaultKeyset::TPM_WRAPPED)
      && serialized.has_tpm_key()) {
    EnsureTpm();
    if (wrap_flags) {
      *wrap_flags |= SerializedVaultKeyset::TPM_WRAPPED;
    }
    if (tpm_ == NULL) {
      LOG(ERROR) << "Vault keyset is wrapped by the TPM, but the TPM is "
                 << "unavailable";
      // TODO(fes): Revisit the decision to make this a fatal error.  Having
      // this be a fatal error means the user's crytpohome will be deleted and
      // recreated.
      if (error) {
        *error = CE_TPM_FATAL;
      }
      return false;
    }
    // Check if the public key for this keyset matches the public key on this
    // TPM.  If not, we cannot recover.
    if (serialized.has_tpm_public_key_hash()) {
      SecureBlob serialized_pub_key_hash(
          serialized.tpm_public_key_hash().length());
      serialized.tpm_public_key_hash().copy(
          static_cast<char*>(serialized_pub_key_hash.data()),
          serialized.tpm_public_key_hash().length(),
          0);
      SecureBlob pub_key;
      if (!tpm_->GetPublicKey(&pub_key)) {
        LOG(ERROR) << "Unable to get the cryptohome public key from the TPM.";
        // TODO(fes): Revisit the decision to make this a fatal error.  Having
        // this be a fatal error means the user's crytpohome will be deleted and
        // recreated.
        if (error) {
          *error = CE_TPM_FATAL;
        }
        return false;
      }
      SecureBlob pub_key_hash;
      GetSha1(pub_key, 0, pub_key.size(), &pub_key_hash);
      if ((serialized_pub_key_hash.size() != pub_key_hash.size()) ||
          (memcmp(serialized_pub_key_hash.data(),
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
    if (!tpm_->Decrypt(tpm_key, vault_wrapper, rounds, salt,
                       &local_vault_wrapper)) {
      LOG(ERROR) << "The TPM failed to unwrap the intermediate key with the "
                 << "supplied credentials";
      if (error) {
        *error = CE_TPM_CRYPTO;
      }
      return false;
    }
  }

  SecureBlob plain_text;
  if (serialized.flags() & SerializedVaultKeyset::SCRYPT_WRAPPED) {
    if (wrap_flags) {
      *wrap_flags |= SerializedVaultKeyset::SCRYPT_WRAPPED;
    }
    size_t out_len = wrapped_keyset.size();
    SecureBlob decrypted(out_len);
    if (scryptdec_buf(
            static_cast<const uint8_t*>(local_wrapped_keyset.const_data()),
            local_wrapped_keyset.size(),
            static_cast<uint8_t*>(decrypted.data()),
            &out_len,
            static_cast<const uint8_t*>(&vault_wrapper[0]),
            vault_wrapper.size(),
            kScryptMaxMem,
            100.0,
            kScryptMaxDecryptTime)) {
      LOG(ERROR) << "Scrypt decryption failed";
      if (error) {
        *error = CE_SCRYPT_CRYPTO;
      }
      return false;
    }
    decrypted.resize(out_len);
    SecureBlob hash;
    int hash_offset = decrypted.size() - SHA_DIGEST_LENGTH;
    GetSha1(decrypted, 0, hash_offset, &hash);
    if (memcmp(hash.data(), &decrypted[hash_offset], hash.size())) {
      LOG(ERROR) << "Scrypt hash verification failed";
      if (error) {
        *error = CE_SCRYPT_CRYPTO;
      }
      return false;
    }
    decrypted.resize(hash_offset);
    plain_text.swap(decrypted);
  } else {
    SecureBlob wrapper_key;
    SecureBlob iv;
    if (!PasskeyToAesKey(local_vault_wrapper,
                         salt,
                         rounds,
                         &wrapper_key,
                         &iv)) {
      LOG(ERROR) << "Failure converting passkey to key";
      if (error) {
        *error = CE_OTHER_FATAL;
      }
      return false;
    }

    if (!UnwrapAes(local_wrapped_keyset, 0, local_wrapped_keyset.size(),
                   wrapper_key, iv, PADDING_CRYPTOHOME_DEFAULT, &plain_text)) {
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

bool Crypto::WrapVaultKeyset(const VaultKeyset& vault_keyset,
                             const SecureBlob& vault_wrapper,
                             const SecureBlob& vault_wrapper_salt,
                             SecureBlob* wrapped_keyset) const {
  SecureBlob keyset_blob;
  if (!vault_keyset.ToKeysBlob(&keyset_blob)) {
    LOG(ERROR) << "Failure serializing keyset to buffer";
    return false;
  }

  int rounds = kDefaultPasswordRounds;
  bool tpm_wrapped = false;
  SecureBlob local_vault_wrapper(vault_wrapper.begin(), vault_wrapper.end());
  SecureBlob tpm_key;
  // Check if the vault keyset was TPM-wrapped
  if (use_tpm_) {
    EnsureTpm();
  }
  // TODO(fes): For now, we allow fall-through to non-TPM-wrapped if tpm_ is
  // NULL.  If/when we make TPM-wrapped the default, we'll want to fail
  // appropriately if the TPM is not available.
  if (use_tpm_ && tpm_ != NULL) {
    GetSecureRandom(static_cast<unsigned char*>(local_vault_wrapper.data()),
                    local_vault_wrapper.size());
    if (tpm_->Encrypt(local_vault_wrapper, vault_wrapper, rounds,
                      vault_wrapper_salt, &tpm_key)) {
      tpm_wrapped = true;
    } else {
      local_vault_wrapper.resize(vault_wrapper.size());
      memcpy(local_vault_wrapper.data(), vault_wrapper.const_data(),
             vault_wrapper.size());
      LOG(ERROR) << "The TPM failed to wrap the intermediate key with the "
                 << "supplied credentials.  The vault keyset will not be "
                 << "further secured by the TPM.";
    }
  }

  SecureBlob cipher_text;
  bool scrypt_wrapped = false;
  // If scrypt is enabled, and the keyset wasn't TPM wrapped, then we use scrypt
  // to encrypt the keyset.  Otherwise, we use regular AES.
  if (fallback_to_scrypt_ && !tpm_wrapped) {
    // Append the SHA1 hash of the keyset blob
    SecureBlob hash;
    GetSha1(keyset_blob, 0, keyset_blob.size(), &hash);
    SecureBlob local_keyset_blob(keyset_blob.size() + hash.size());
    memcpy(&local_keyset_blob[0], keyset_blob.const_data(), keyset_blob.size());
    memcpy(&local_keyset_blob[keyset_blob.size()], hash.data(), hash.size());
    cipher_text.resize(local_keyset_blob.size() + kScryptHeaderLength);
     if (scryptenc_buf(
            static_cast<const uint8_t*>(local_keyset_blob.const_data()),
            local_keyset_blob.size(),
            static_cast<uint8_t*>(cipher_text.data()),
            static_cast<const uint8_t*>(&vault_wrapper[0]),
            vault_wrapper.size(),
            kScryptMaxMem,
            100.0,
            kScryptMaxEncryptTime)) {
      LOG(ERROR) << "Scrypt encryption failed";
      return false;
    }
    scrypt_wrapped = true;
  } else {
    SecureBlob wrapper_key;
    SecureBlob iv;
    if (!PasskeyToAesKey(local_vault_wrapper, vault_wrapper_salt,
                         rounds, &wrapper_key, &iv)) {
      LOG(ERROR) << "Failure converting passkey to key";
      return false;
    }

    if (!WrapAes(keyset_blob, 0, keyset_blob.size(), wrapper_key, iv,
                 PADDING_CRYPTOHOME_DEFAULT, &cipher_text)) {
      LOG(ERROR) << "AES encryption failed.";
      return false;
    }
  }

  SerializedVaultKeyset serialized;
  int keyset_flags = SerializedVaultKeyset::NONE;
  if (tpm_wrapped) {
    // Store the TPM-encrypted key
    keyset_flags = SerializedVaultKeyset::TPM_WRAPPED;
    serialized.set_tpm_key(tpm_key.const_data(),
                           tpm_key.size());
    // Store the hash of the cryptohome public key
    SecureBlob pub_key;
    if (tpm_->GetPublicKey(&pub_key)) {
      SecureBlob pub_key_hash;
      GetSha1(pub_key, 0, pub_key.size(), &pub_key_hash);
      serialized.set_tpm_public_key_hash(pub_key_hash.const_data(),
                                         pub_key_hash.size());
    }
  }
  if (scrypt_wrapped) {
    keyset_flags |= SerializedVaultKeyset::SCRYPT_WRAPPED;
  }
  serialized.set_flags(keyset_flags);
  serialized.set_salt(vault_wrapper_salt.const_data(),
                      vault_wrapper_salt.size());
  serialized.set_wrapped_keyset(cipher_text.const_data(), cipher_text.size());
  serialized.set_password_rounds(rounds);

  SecureBlob final_blob(serialized.ByteSize());
  serialized.SerializeWithCachedSizesToArray(
      static_cast<google::protobuf::uint8*>(final_blob.data()));
  wrapped_keyset->swap(final_blob);
  return true;
}

bool Crypto::UnwrapVaultKeysetOld(const chromeos::Blob& wrapped_keyset,
                                  const chromeos::Blob& vault_wrapper,
                                  VaultKeyset* vault_keyset) const {
  unsigned int header_size = kOpenSSLMagic.length() + PKCS5_SALT_LEN;

  if (wrapped_keyset.size() < header_size) {
    LOG(ERROR) << "Master key file too short";
    return false;
  }

  // Grab the salt used in converting the passkey to a key (OpenSSL
  // passkey-encrypted files have the format:
  // Salted__<8-byte-salt><ciphertext>)
  SecureBlob salt(PKCS5_SALT_LEN);
  memcpy(salt.data(), &wrapped_keyset[kOpenSSLMagic.length()], PKCS5_SALT_LEN);

  SecureBlob wrapper_key;
  SecureBlob iv;
  if (!PasskeyToAesKey(vault_wrapper, salt, kDefaultLegacyPasswordRounds,
                       &wrapper_key, &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  SecureBlob plain_text;
  if (!UnwrapAesSpecifyBlockMode(wrapped_keyset,
                                 header_size,
                                 wrapped_keyset.size() - header_size,
                                 wrapper_key,
                                 iv,
                                 PADDING_LIBRARY_DEFAULT,
                                 ECB,
                                 &plain_text)) {
    LOG(ERROR) << "AES decryption failed.";
    return false;
  }

  OldVaultKeyset old_keyset;
  old_keyset.AssignBuffer(plain_text);

  vault_keyset->FromVaultKeyset(old_keyset);
  return true;
}

bool Crypto::WrapVaultKeysetOld(const VaultKeyset& vault_keyset,
                                const SecureBlob& vault_wrapper,
                                const SecureBlob& vault_wrapper_salt,
                                SecureBlob* wrapped_keyset) const {
  OldVaultKeyset old_keyset;
  old_keyset.FromVaultKeyset(vault_keyset);

  SecureBlob keyset_blob;
  if (!old_keyset.ToBuffer(&keyset_blob)) {
    LOG(ERROR) << "Failure serializing keyset to buffer";
    return false;
  }

  SecureBlob wrapper_key;
  SecureBlob iv;
  if (!PasskeyToAesKey(vault_wrapper, vault_wrapper_salt,
                       kDefaultLegacyPasswordRounds, &wrapper_key, &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  SecureBlob cipher_text;
  if (!WrapAesSpecifyBlockMode(keyset_blob, 0, keyset_blob.size(), wrapper_key,
                               iv, PADDING_LIBRARY_DEFAULT, ECB,
                               &cipher_text)) {
    LOG(ERROR) << "AES encryption failed.";
    return false;
  }

  unsigned int header_size = kOpenSSLMagic.length() + PKCS5_SALT_LEN;
  SecureBlob final_blob(header_size + cipher_text.size());
  memcpy(&final_blob[0], kOpenSSLMagic.c_str(), kOpenSSLMagic.length());
  memcpy(&final_blob[kOpenSSLMagic.length()], vault_wrapper_salt.const_data(),
         PKCS5_SALT_LEN);
  memcpy(&final_blob[header_size], cipher_text.data(), cipher_text.size());

  wrapped_keyset->swap(final_blob);
  return true;
}

} // namespace cryptohome
