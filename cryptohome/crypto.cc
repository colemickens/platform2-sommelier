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
#include <scrypt/crypto_scrypt.h>
#include <scrypt/scryptenc.h>
}

#include "cryptohome_common.h"
#include "cryptolib.h"
#include "platform.h"
#include "username_passkey.h"

// Included last because they both have conflicting defines :(
extern "C" {
#include <ecryptfs.h>
#include <keyutils.h>
}

using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

// The OpenSSL "magic" value that prefixes the encrypted data blob.  This is
// only used in migrating legacy keysets, which were stored in this manner.
const std::string kOpenSSLMagic = "Salted__";

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

// AES key size in bytes (256-bit).  This key size is used for all key creation,
// though we currently only use 128 bits for the eCryptfs File Encryption Key
// (FEK).  Larger than 128-bit has too great of a CPU overhead on unaccelerated
// architectures.
const unsigned int kDefaultAesKeySize = 32;

// Maximum size of the salt file.
const int64 Crypto::kSaltMax = (1 << 20);  // 1 MB

Crypto::Crypto()
    : use_tpm_(false),
      tpm_(NULL) {
}

Crypto::~Crypto() {
}

bool Crypto::Init(Platform* platform) {
  if (use_tpm_ && tpm_ == NULL) {
    tpm_ = Tpm::GetSingleton();
  }
  if (tpm_) {
    tpm_->Init(platform, true);
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

bool Crypto::PasskeyToTokenAuthData(const chromeos::Blob& passkey,
                                    const FilePath& salt_file,
                                    SecureBlob* auth_data) const {
  // Use the scrypt algorithm to derive auth data from the passkey.
  const size_t kAuthDataSizeBytes = 32;
  // The following scrypt parameters are based on Colin Percival's interactive
  // login example in http://www.tarsnap.com/scrypt/scrypt-slides.pdf and
  // adjusted for ~100ms performance on slower models. The performance is
  // dependant on CPU and memory performance.
  const uint64_t kScryptParameterN = (1 << 11);
  const uint32_t kScryptParameterR = 8;
  const uint32_t kScryptParameterP = 1;
  const unsigned int kSaltLength = 32;
  SecureBlob salt;
  if (!GetOrCreateSalt(salt_file, kSaltLength, false, &salt)) {
    LOG(ERROR) << "Failed to get authorization data salt.";
    return false;
  }
  SecureBlob local_auth_data;
  local_auth_data.resize(kAuthDataSizeBytes);
  if (0 != crypto_scrypt(&passkey[0],
                         passkey.size(),
                         &salt[0],
                         salt.size(),
                         kScryptParameterN,
                         kScryptParameterR,
                         kScryptParameterP,
                         &local_auth_data[0],
                         kAuthDataSizeBytes)) {
    LOG(ERROR) << "Scrypt key derivation failed.";
    return false;
  }
  auth_data->swap(local_auth_data);
  return true;
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
    LOG(ERROR) << "Creating new salt at " << path.value()
               << " (" << force << ", " << file_len << ")";
    // If this salt doesn't exist, automatically create it
    local_salt.resize(length);
    CryptoLib::GetSecureRandom(static_cast<unsigned char*>(local_salt.data()),
                               local_salt.size());
    unsigned int data_written = file_util::WriteFile(path,
        static_cast<const char*>(local_salt.const_data()),
        length);
    if (data_written != length) {
      LOG(ERROR) << "Could not write user salt: " << data_written
                 << " != " << length;
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
  keyctl(KEYCTL_CLEAR, KEY_SPEC_USER_KEYRING);
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
  CryptoLib::AsciiEncodeToBuffer(md_value,
                                 static_cast<char*>(local_passkey.data()),
                                 local_passkey.size());
  passkey->swap(local_passkey);
}

bool Crypto::IsTPMPubkeyHash(const string& hash,
                             CryptoError* error) const {
  SecureBlob pub_key_hash;
  Tpm::TpmRetryAction retry_action;
  retry_action = tpm_->GetPublicKeyHash(&pub_key_hash);
  if (retry_action != Tpm::RetryNone) {
    LOG(ERROR) << "Unable to get the cryptohome public key from the TPM.";
    if (error)
      // This is a fatal error only if we don't get a transient error code.
      *error = TpmErrorToCrypto(retry_action);
    return false;
  }
  if ((hash.size() != pub_key_hash.size()) ||
      (chromeos::SafeMemcmp(hash.data(),
              pub_key_hash.data(),
              pub_key_hash.size()))) {
    if (error)
      *error = CE_TPM_FATAL;
    return false;
  }

  return true;
}

bool Crypto::DecryptTPM(const SerializedVaultKeyset& serialized,
                        const SecureBlob& vault_key,
                        CryptoError* error,
                        VaultKeyset* keyset) const {
  CHECK(tpm_);
  CHECK(serialized.flags() & SerializedVaultKeyset::TPM_WRAPPED);
  SecureBlob local_encrypted_keyset(serialized.wrapped_keyset().length());
  serialized.wrapped_keyset().copy(
      static_cast<char*>(local_encrypted_keyset.data()),
      serialized.wrapped_keyset().length(), 0);
  SecureBlob salt(serialized.salt().length());
  serialized.salt().copy(static_cast<char*>(salt.data()),
                         serialized.salt().length(), 0);
  SecureBlob local_vault_key(vault_key.begin(), vault_key.end());

  if (!serialized.has_tpm_key()) {
    LOG(ERROR) << "Decrypting with TPM, but no tpm key present";
    if (error)
      *error = CE_TPM_FATAL;
    return false;
  }

  // If the TPM is enabled but not owned, and the keyset is TPM wrapped, then
  // it means the TPM has been cleared since the last login, and is not
  // re-owned.  In this case, the SRK is cleared and we cannot recover the
  // keyset.
  if (tpm_->IsEnabled() && !tpm_->IsOwned()) {
    LOG(ERROR) << "Fatal error--the TPM is enabled but not owned, and this "
               << "keyset was wrapped by the TPM.  It is impossible to "
               << "recover this keyset.";
    if (error)
      *error = CE_TPM_FATAL;
    return false;
  }

  Crypto::CryptoError local_error = EnsureTpm(false);
  if (!tpm_->IsConnected()) {
    LOG(ERROR) << "Vault keyset is wrapped by the TPM, but the TPM is "
               << "unavailable";
    if (error)
      *error = local_error;
    return false;
  }

  unsigned int rounds;
  if (serialized.has_password_rounds()) {
    rounds = serialized.password_rounds();
  } else {
    rounds = kDefaultLegacyPasswordRounds;
  }

  if (serialized.has_tpm_public_key_hash()) {
    if (!IsTPMPubkeyHash(serialized.tpm_public_key_hash(), error)) {
      LOG(ERROR) << "TPM public key hash mismatch.";
      return false;
    }
  }

  Tpm::TpmRetryAction retry_action;
  SecureBlob tpm_key(serialized.tpm_key().length());
  serialized.tpm_key().copy(static_cast<char*>(tpm_key.data()),
                            serialized.tpm_key().length(), 0);
  SecureBlob key;
  CryptoLib::PasskeyToAesKey(vault_key, salt, rounds, &key, NULL);
  if (!tpm_->Decrypt(tpm_key, key, &local_vault_key, &retry_action)) {
    LOG(ERROR) << "The TPM failed to unwrap the intermediate key with the "
               << "supplied credentials";
    if (error)
      // This is a fatal error only if we don't get a transient error code.
      *error = TpmErrorToCrypto(retry_action);
    return false;
  }

  SecureBlob aes_key;
  SecureBlob iv;
  SecureBlob plain_text;
  if (!CryptoLib::PasskeyToAesKey(local_vault_key,
                                  salt,
                                  rounds,
                                  &aes_key,
                                  &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    if (error)
      *error = CE_OTHER_FATAL;
    return false;
  }

  if (!CryptoLib::AesDecrypt(local_encrypted_keyset, aes_key, iv,
                             &plain_text)) {
    LOG(ERROR) << "AES decryption failed.";
    if (error)
      *error = CE_OTHER_CRYPTO;
    return false;
  }

  keyset->FromKeysBlob(plain_text);
  if (!serialized.has_tpm_public_key_hash() && error)
    *error = CE_NO_PUBLIC_KEY_HASH;
  return true;
}

bool Crypto::DecryptScrypt(const SerializedVaultKeyset& serialized,
                           const SecureBlob& key,
                           CryptoError* error,
                           VaultKeyset* keyset) const {
  SecureBlob blob(serialized.wrapped_keyset().length());
  serialized.wrapped_keyset().copy(static_cast<char*>(blob.data()),
                                   blob.size(), 0);
  size_t out_len = blob.size();
  SecureBlob decrypted(out_len);
  int scrypt_rc;
  if ((scrypt_rc = scryptdec_buf(
          static_cast<const uint8_t*>(blob.const_data()),
          blob.size(),
          static_cast<uint8_t*>(decrypted.data()),
          &out_len,
          static_cast<const uint8_t*>(key.const_data()),
          key.size(),
          kScryptMaxMem,
          100.0,
          kScryptMaxDecryptTime))) {
    LOG(ERROR) << "Scrypt decryption failed: " << scrypt_rc;
    if (error)
      *error = CE_SCRYPT_CRYPTO;
    return false;
  }
  // TODO(ellyjones): size underflow?
  decrypted.resize(out_len);
  SecureBlob included_hash(SHA_DIGEST_LENGTH);
  if (decrypted.size() < SHA_DIGEST_LENGTH) {
    LOG(ERROR) << "Message length underflow: " << decrypted.size() << " bytes?";
    return false;
  }
  memcpy(&included_hash[0], &decrypted[decrypted.size() - SHA_DIGEST_LENGTH],
         SHA_DIGEST_LENGTH);
  decrypted.resize(decrypted.size() - SHA_DIGEST_LENGTH);
  chromeos::Blob hash = CryptoLib::Sha1(decrypted);
  if (chromeos::SafeMemcmp(hash.data(), &included_hash[0],
                           hash.size())) {
    LOG(ERROR) << "Scrypt hash verification failed";
    if (error) {
      *error = CE_SCRYPT_CRYPTO;
    }
    return false;
  }
  keyset->FromKeysBlob(decrypted);
  return true;
}

bool Crypto::DecryptVaultKeyset(const SerializedVaultKeyset& serialized,
                                const SecureBlob& vault_key,
                                unsigned int* crypt_flags, CryptoError* error,
                                VaultKeyset* vault_keyset) const {
  if (crypt_flags)
    *crypt_flags = serialized.flags();
  if (error)
    *error = CE_NONE;

  // Check if the vault keyset was TPM-wrapped
  if (serialized.flags() & SerializedVaultKeyset::TPM_WRAPPED) {
    return DecryptTPM(serialized, vault_key, error, vault_keyset);
  } else if (serialized.flags() & SerializedVaultKeyset::SCRYPT_WRAPPED) {
    return DecryptScrypt(serialized, vault_key, error, vault_keyset);
  } else {
    LOG(ERROR) << "Keyset wrapped with neither TPM nor Scrypt?";
    return false;
  }
}

bool Crypto::EncryptTPM(const SecureBlob& blob,
                        const SecureBlob& key,
                        const SecureBlob& salt,
                        SerializedVaultKeyset* serialized) const {
  unsigned int rounds = kDefaultPasswordRounds;
  if (!use_tpm_)
    return false;
  EnsureTpm(false);
  if (tpm_ == NULL || !tpm_->IsConnected())
    return false;
  SecureBlob local_blob(kDefaultAesKeySize);
  CryptoLib::GetSecureRandom(static_cast<unsigned char*>(local_blob.data()),
                             local_blob.size());
  Tpm::TpmRetryAction retry_action;
  SecureBlob tpm_key;
  SecureBlob derived_key;
  CryptoLib::PasskeyToAesKey(key, salt, rounds, &derived_key, NULL);
  // Encrypt the VKK using the TPM and the user's passkey.  The output is an
  // encrypted blob in tpm_key, which is stored in the serialized vault
  // keyset.
  if (!tpm_->Encrypt(local_blob, derived_key, &tpm_key, &retry_action)) {
    LOG(ERROR) << "Failed to wrap vkk with creds.";
    return false;
  }

  SecureBlob aes_key;
  SecureBlob iv;
  SecureBlob cipher_text;
  if (!CryptoLib::PasskeyToAesKey(local_blob, salt, rounds, &aes_key, &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  if (!CryptoLib::AesEncrypt(blob, aes_key, iv, &cipher_text)) {
    LOG(ERROR) << "AES encryption failed.";
    return false;
  }

  // Allow this to fail.  It is not absolutely necessary; it allows us to
  // detect a TPM clear.  If this fails due to a transient issue, then on next
  // successful login, the vault keyset will be re-saved anyway.
  SecureBlob pub_key_hash;
  if (tpm_->GetPublicKeyHash(&pub_key_hash) == Tpm::RetryNone)
    serialized->set_tpm_public_key_hash(pub_key_hash.const_data(),
                                        pub_key_hash.size());

  unsigned int flags = serialized->flags();
  serialized->set_password_rounds(rounds);
  serialized->set_flags(flags | SerializedVaultKeyset::TPM_WRAPPED);
  serialized->set_tpm_key(tpm_key.const_data(), tpm_key.size());
  serialized->set_wrapped_keyset(cipher_text.const_data(), cipher_text.size());
  return true;
}

bool Crypto::EncryptScrypt(const SecureBlob& blob,
                           const SecureBlob& key,
                           SerializedVaultKeyset* serialized) const {
  // Append the SHA1 hash of the keyset blob
  SecureBlob cipher_text;
  SecureBlob hash = CryptoLib::Sha1(blob);
  SecureBlob local_blob(blob.size() + hash.size());
  memcpy(&local_blob[0], blob.const_data(), blob.size());
  memcpy(&local_blob[blob.size()], hash.data(), hash.size());
  cipher_text.resize(local_blob.size() + kScryptHeaderLength);
  int scrypt_rc;
  if ((scrypt_rc = scryptenc_buf(
          static_cast<const uint8_t*>(local_blob.const_data()),
          local_blob.size(),
          static_cast<uint8_t*>(cipher_text.data()),
          static_cast<const uint8_t*>(&key[0]),
          key.size(),
          kScryptMaxMem,
          100.0,
          kScryptMaxEncryptTime))) {
    LOG(ERROR) << "Scrypt encryption failed: " << scrypt_rc;
    return false;
  }
  unsigned int flags = serialized->flags();
  serialized->set_flags(flags | SerializedVaultKeyset::SCRYPT_WRAPPED);
  serialized->set_wrapped_keyset(cipher_text.const_data(), cipher_text.size());
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

  if (!EncryptTPM(keyset_blob, vault_key, vault_key_salt, serialized))
    if (!EncryptScrypt(keyset_blob, vault_key, serialized))
      return false;

  serialized->set_salt(vault_key_salt.const_data(),
                       vault_key_salt.size());
  return true;
}

}  // namespace cryptohome
