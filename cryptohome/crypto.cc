// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Crypto

#include "crypto.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/utility.h>

#include "cryptohome_common.h"
#include "platform.h"
#include "username_passkey.h"

// Included last because it has conflicting defines
extern "C" {
#include <ecryptfs.h>
}

using std::string;

namespace cryptohome {

const std::string kDefaultEntropySource = "/dev/urandom";
const std::string kOpenSSLMagic = "Salted__";

Crypto::Crypto()
    : entropy_source_(kDefaultEntropySource) {
}

Crypto::~Crypto() {
}

void Crypto::GetSecureRandom(unsigned char *rand, int length) const {
  // TODO(fes): Get assistance from the TPM when it is available
  // Seed the OpenSSL random number generator until it is happy
  while (!RAND_status()) {
    char buffer[256];
    file_util::ReadFile(FilePath(entropy_source_), buffer, sizeof(buffer));
    RAND_add(buffer, sizeof(buffer), sizeof(buffer));
  }

  // Have OpenSSL generate the random bytes
  RAND_bytes(rand, length);
}

bool Crypto::UnwrapVaultKeyset(const chromeos::Blob& wrapped_keyset,
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
  unsigned char salt[PKCS5_SALT_LEN];
  memcpy(salt, &wrapped_keyset[kOpenSSLMagic.length()], PKCS5_SALT_LEN);

  unsigned char wrapper_key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];

  // Convert the passkey to a key encryption key
  if (!EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, &vault_wrapper[0],
                          vault_wrapper.size(), 1, wrapper_key, iv)) {
    LOG(ERROR) << "Failure converting bytes to key";
    return false;
  }

  int cipher_text_size = wrapped_keyset.size() - header_size;
  SecureBlob plain_text(cipher_text_size);

  int final_size = 0;
  int decrypt_size = plain_text.size();

  // Do the actual decryption
  EVP_CIPHER_CTX d_ctx;
  EVP_CIPHER_CTX_init(&d_ctx);
  EVP_DecryptInit_ex(&d_ctx, EVP_aes_256_ecb(), NULL, wrapper_key, iv);
  if (!EVP_DecryptUpdate(&d_ctx, &plain_text[0], &decrypt_size,
                        &wrapped_keyset[header_size],
                        cipher_text_size)) {
    LOG(ERROR) << "DecryptUpdate failed";
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

    return false;
  }
  final_size += decrypt_size;

  plain_text.resize(final_size);

  if (plain_text.size() != VaultKeyset::SerializedSize()) {
    LOG(ERROR) << "Plain text was not the correct size: " << plain_text.size()
               << ", expected: " << VaultKeyset::SerializedSize();
    return false;
  }

  (*vault_keyset).AssignBuffer(plain_text);
  return true;
}

bool Crypto::WrapVaultKeyset(const VaultKeyset& vault_keyset,
                             const SecureBlob& vault_wrapper,
                             const SecureBlob& vault_wrapper_salt,
                             SecureBlob* wrapped_keyset) const {
  unsigned char wrapper_key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];

  if (vault_wrapper_salt.size() < PKCS5_SALT_LEN) {
    LOG(ERROR) << "Vault wrapper salt was not the correct length";
    return false;
  }

  // Convert the passkey wrapper to a key encryption key
  if (!EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), &vault_wrapper_salt[0],
                      &vault_wrapper[0], vault_wrapper.size(), 1,
                      wrapper_key, iv)) {
    LOG(ERROR) << "Failure converting bytes to key";
    return false;
  }

  SecureBlob keyset_blob;
  if (!vault_keyset.ToBuffer(&keyset_blob)) {
    LOG(ERROR) << "Failure serializing keyset to buffer";
    return false;
  }

  // Store the salt and encrypt the master key
  unsigned int header_size = kOpenSSLMagic.length() + PKCS5_SALT_LEN;
  SecureBlob cipher_text(header_size
                         + keyset_blob.size()
                         + EVP_CIPHER_block_size(EVP_aes_256_ecb()));
  memcpy(&cipher_text[0], kOpenSSLMagic.c_str(), kOpenSSLMagic.length());
  memcpy(&cipher_text[kOpenSSLMagic.length()], vault_wrapper_salt.const_data(),
         PKCS5_SALT_LEN);

  int current_size = header_size;
  int encrypt_size = 0;
  EVP_CIPHER_CTX e_ctx;
  EVP_CIPHER_CTX_init(&e_ctx);

  // Encrypt the keyset
  EVP_EncryptInit_ex(&e_ctx, EVP_aes_256_ecb(), NULL, wrapper_key, iv);
  if (!EVP_EncryptUpdate(&e_ctx, &cipher_text[current_size], &encrypt_size,
                         &keyset_blob[0],
                         keyset_blob.size())) {
    LOG(ERROR) << "EncryptUpdate failed";
    chromeos::SecureMemset(wrapper_key, 0, sizeof(wrapper_key));
    return false;
  }
  current_size += encrypt_size;
  encrypt_size = 0;

  // Finish the encryption
  if (!EVP_EncryptFinal_ex(&e_ctx, &cipher_text[current_size], &encrypt_size)) {
    LOG(ERROR) << "EncryptFinal failed";
    chromeos::SecureMemset(wrapper_key, 0, sizeof(wrapper_key));
    return false;
  }
  current_size += encrypt_size;
  cipher_text.resize(current_size);

  chromeos::SecureMemset(wrapper_key, 0, sizeof(wrapper_key));

  wrapped_keyset->swap(cipher_text);
  return true;
}

void Crypto::PasskeyToWrapper(const chromeos::Blob& passkey,
                              const chromeos::Blob& salt, int iters,
                              SecureBlob* wrapper) const {
  // TODO(fes): Update this when TPM support is available, or use a memory-
  // bound strengthening algorithm.
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

} // namespace cryptohome
