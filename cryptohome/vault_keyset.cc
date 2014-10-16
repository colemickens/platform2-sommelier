// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>

#include <base/logging.h>
#include <chromeos/secure_blob.h>

#include "cryptohome/crypto.h"
#include "cryptohome/cryptohome_common.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/platform.h"
#include "cryptohome/vault_keyset.h"

using chromeos::SecureBlob;

namespace {
const mode_t kVaultFilePermissions = 0600;
}

namespace cryptohome {

VaultKeyset::VaultKeyset()
    : platform_(NULL), crypto_(NULL), loaded_(false), encrypted_(false),
      legacy_index_(-1) {
}

VaultKeyset::~VaultKeyset() {
}

void VaultKeyset::Initialize(Platform* platform, Crypto* crypto) {
  platform_ = platform;
  crypto_ = crypto;
}

void VaultKeyset::FromVaultKeyset(const VaultKeyset& vault_keyset) {
  fek_.resize(vault_keyset.fek_.size());
  memcpy(fek_.data(), vault_keyset.fek_.const_data(), fek_.size());

  fek_sig_.resize(vault_keyset.fek_sig_.size());
  memcpy(fek_sig_.data(), vault_keyset.fek_sig_.const_data(), fek_sig_.size());

  fek_salt_.resize(vault_keyset.fek_salt_.size());
  memcpy(fek_salt_.data(), vault_keyset.fek_salt_.const_data(),
         fek_salt_.size());

  fnek_.resize(vault_keyset.fnek_.size());
  memcpy(fnek_.data(), vault_keyset.fnek_.const_data(), fnek_.size());

  fnek_sig_.resize(vault_keyset.fnek_sig_.size());
  memcpy(fnek_sig_.data(), vault_keyset.fnek_sig_.const_data(),
         fnek_sig_.size());

  fnek_salt_.resize(vault_keyset.fnek_salt_.size());
  memcpy(fnek_salt_.data(), vault_keyset.fnek_salt_.const_data(),
         fnek_salt_.size());
}

void VaultKeyset::FromKeys(const VaultKeysetKeys& keys) {
  fek_.resize(sizeof(keys.fek));
  memcpy(&fek_[0], keys.fek, fek_.size());
  fek_sig_.resize(sizeof(keys.fek_sig));
  memcpy(&fek_sig_[0], keys.fek_sig, fek_sig_.size());
  fek_salt_.resize(sizeof(keys.fek_salt));
  memcpy(&fek_salt_[0], keys.fek_salt, fek_salt_.size());
  fnek_.resize(sizeof(keys.fnek));
  memcpy(&fnek_[0], keys.fnek, fnek_.size());
  fnek_sig_.resize(sizeof(keys.fnek_sig));
  memcpy(&fnek_sig_[0], keys.fnek_sig, fnek_sig_.size());
  fnek_salt_.resize(sizeof(keys.fnek_salt));
  memcpy(&fnek_salt_[0], keys.fnek_salt, fnek_salt_.size());
}

bool VaultKeyset::FromKeysBlob(const SecureBlob& keys_blob) {
  if (keys_blob.size() != sizeof(VaultKeysetKeys)) {
    return false;
  }
  VaultKeysetKeys keys;
  memcpy(&keys, keys_blob.const_data(), sizeof(keys));

  FromKeys(keys);

  chromeos::SecureMemset(&keys, 0, sizeof(keys));
  return true;
}

bool VaultKeyset::ToKeys(VaultKeysetKeys* keys) const {
  chromeos::SecureMemset(keys, 0, sizeof(VaultKeysetKeys));
  if (fek_.size() != sizeof(keys->fek)) {
    return false;
  }
  memcpy(keys->fek, fek_.const_data(), sizeof(keys->fek));
  if (fek_sig_.size() != sizeof(keys->fek_sig)) {
    return false;
  }
  memcpy(keys->fek_sig, fek_sig_.const_data(), sizeof(keys->fek_sig));
  if (fek_salt_.size() != sizeof(keys->fek_salt)) {
    return false;
  }
  memcpy(keys->fek_salt, fek_salt_.const_data(), sizeof(keys->fek_salt));
  if (fnek_.size() != sizeof(keys->fnek)) {
    return false;
  }
  memcpy(keys->fnek, fnek_.const_data(), sizeof(keys->fnek));
  if (fnek_sig_.size() != sizeof(keys->fnek_sig)) {
    return false;
  }
  memcpy(keys->fnek_sig, fnek_sig_.const_data(), sizeof(keys->fnek_sig));
  if (fnek_salt_.size() != sizeof(keys->fnek_salt)) {
    return false;
  }
  memcpy(keys->fnek_salt, fnek_salt_.const_data(), sizeof(keys->fnek_salt));

  return true;
}

bool VaultKeyset::ToKeysBlob(SecureBlob* keys_blob) const {
  VaultKeysetKeys keys;
  if (!ToKeys(&keys)) {
    return false;
  }

  SecureBlob local_buffer(sizeof(keys));
  memcpy(static_cast<unsigned char*>(local_buffer.data()), &keys,
         sizeof(keys));
  keys_blob->swap(local_buffer);
  return true;
}

void VaultKeyset::CreateRandomChapsKey() {
  chaps_key_.clear_contents();
  chaps_key_.resize(CRYPTOHOME_CHAPS_KEY_LENGTH);
  CryptoLib::GetSecureRandom(&chaps_key_[0], chaps_key_.size());
}

void VaultKeyset::CreateRandom() {
  CHECK(crypto_);
  fek_.resize(CRYPTOHOME_DEFAULT_KEY_SIZE);
  CryptoLib::GetSecureRandom(&fek_[0], fek_.size());

  fek_sig_.resize(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE);
  CryptoLib::GetSecureRandom(&fek_sig_[0], fek_sig_.size());

  fek_salt_.resize(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  CryptoLib::GetSecureRandom(&fek_salt_[0], fek_salt_.size());

  fnek_.resize(CRYPTOHOME_DEFAULT_KEY_SIZE);
  CryptoLib::GetSecureRandom(&fnek_[0], fnek_.size());

  fnek_sig_.resize(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE);
  CryptoLib::GetSecureRandom(&fnek_sig_[0], fnek_sig_.size());

  fnek_salt_.resize(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  CryptoLib::GetSecureRandom(&fnek_salt_[0], fnek_salt_.size());

  CreateRandomChapsKey();
}

const SecureBlob& VaultKeyset::fek() const {
  return fek_;
}

const SecureBlob& VaultKeyset::fek_sig() const {
  return fek_sig_;
}

const SecureBlob& VaultKeyset::fek_salt() const {
  return fek_salt_;
}

const SecureBlob& VaultKeyset::fnek() const {
  return fnek_;
}

const SecureBlob& VaultKeyset::fnek_sig() const {
  return fnek_sig_;
}

const SecureBlob& VaultKeyset::fnek_salt() const {
  return fnek_salt_;
}

void VaultKeyset::set_chaps_key(const SecureBlob& chaps_key) {
  CHECK(chaps_key.size() == CRYPTOHOME_CHAPS_KEY_LENGTH);
  SecureBlob tmp = chaps_key;
  chaps_key_.swap(tmp);
}

void VaultKeyset::clear_chaps_key() {
  CHECK(chaps_key_.size() == CRYPTOHOME_CHAPS_KEY_LENGTH);
  chaps_key_.clear_contents();
  chaps_key_.resize(0);
}

bool VaultKeyset::Load(const std::string& filename) {
  CHECK(platform_);
  SecureBlob contents;
  if (!platform_->ReadFile(filename, &contents))
    return false;

  unsigned char* data = static_cast<unsigned char*>(contents.data());
  serialized_.Clear();  // Ensure a fresh start.
  loaded_ = serialized_.ParseFromArray(data, contents.size());
  // If it was parsed from file, consider it save-able too.
  source_file_.clear();
  if (loaded_) {
    encrypted_ = true;
    source_file_ = filename;
  }
  return loaded_;
}

bool VaultKeyset::Decrypt(const SecureBlob& key) {
  CHECK(crypto_);
  if (!loaded_)
    return false;
  Crypto::CryptoError error;
  bool ok = crypto_->DecryptVaultKeyset(serialized_, key, NULL, &error, this);
  if (!ok && error == Crypto::CE_TPM_COMM_ERROR)
    ok = crypto_->DecryptVaultKeyset(serialized_, key, NULL, &error, this);

  return ok;
}

bool VaultKeyset::Encrypt(const SecureBlob& key) {
  CHECK(crypto_);
  SecureBlob salt(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  unsigned char* salt_buf = static_cast<unsigned char*>(salt.data());
  CryptoLib::GetSecureRandom(salt_buf, salt.size());
  encrypted_ = crypto_->EncryptVaultKeyset(*this, key, salt, &serialized_);
  return encrypted_;
}

bool VaultKeyset::Save(const std::string& filename) {
  CHECK(platform_);
  if (!encrypted_)
    return false;
  SecureBlob contents(serialized_.ByteSize());
  google::protobuf::uint8* buf =
      static_cast<google::protobuf::uint8*>(contents.data());
  serialized_.SerializeWithCachedSizesToArray(buf);

  bool ok = platform_->WriteFileAtomicDurable(filename, contents,
                                              kVaultFilePermissions);
  return ok;
}

}  // namespace cryptohome
