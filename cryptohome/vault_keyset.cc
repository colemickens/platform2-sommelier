// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromeos/utility.h"
#include "cryptohome/cryptohome_common.h"
#include "cryptohome/vault_keyset.h"

namespace cryptohome {

VaultKeyset::VaultKeyset()
    : major_version_(CRYPTOHOME_VAULT_KEYSET_VERSION_MAJOR),
      minor_version_(CRYPTOHOME_VAULT_KEYSET_VERSION_MINOR) {
}

VaultKeyset::VaultKeyset(const SecureBlob& source)
    : major_version_(CRYPTOHOME_VAULT_KEYSET_VERSION_MAJOR),
      minor_version_(CRYPTOHOME_VAULT_KEYSET_VERSION_MINOR) {
  AssignBuffer(source);
}

bool VaultKeyset::AssignBuffer(const SecureBlob& source) {
  if(source.size() < VaultKeyset::SerializedSize()) {
    LOG(ERROR) << "Input buffer is too small.";
    return false;
  }

  int offset = 0;
  VaultKeysetHeader header;
  memcpy(&header, &source[offset], sizeof(header));
  offset += sizeof(header);
  if(memcmp(header.signature, kVaultKeysetSignature,
            sizeof(header.signature))) {
    return false;
  }
  major_version_ = header.major_version;
  minor_version_ = header.minor_version;

  VaultKeysetKeys keys;
  memcpy(&keys, &source[offset], sizeof(keys));
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
  chromeos::SecureMemset(&keys, sizeof(keys), 0);

  return true;
}

SecureBlob VaultKeyset::ToBuffer() const {
  SecureBlob buffer(VaultKeyset::SerializedSize());

  VaultKeysetHeader header;
  memcpy(header.signature, kVaultKeysetSignature, sizeof(header.signature));
  header.major_version = major_version_;
  header.minor_version = minor_version_;
  memcpy(&buffer[0], &header, sizeof(header));

  VaultKeysetKeys keys;
  chromeos::SecureMemset(&keys, sizeof(keys), 0);
  memcpy(keys.fek, &fek_[0],
         CRYPTOHOME_MIN(CRYPTOHOME_DEFAULT_KEY_SIZE, sizeof(keys.fek)));
  memcpy(keys.fek_sig, &fek_sig_[0],
         CRYPTOHOME_MIN(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE,
                        sizeof(keys.fek_sig)));
  memcpy(keys.fek_salt, &fek_salt_[0],
         CRYPTOHOME_MIN(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE,
                        sizeof(keys.fek_salt)));
  memcpy(keys.fnek, &fnek_[0],
         CRYPTOHOME_MIN(CRYPTOHOME_DEFAULT_KEY_SIZE, sizeof(keys.fnek)));
  memcpy(keys.fnek_sig, &fnek_sig_[0],
         CRYPTOHOME_MIN(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE,
                        sizeof(keys.fnek_sig)));
  memcpy(keys.fnek_salt, &fnek_salt_[0],
         CRYPTOHOME_MIN(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE,
                        sizeof(keys.fnek_salt)));
  memcpy(&buffer[sizeof(header)], &keys, sizeof(keys));
  chromeos::SecureMemset(&keys, sizeof(keys), 0);

  return buffer;
}

void VaultKeyset::CreateRandom(const EntropySource& entropy_source) {
  fek_.resize(CRYPTOHOME_DEFAULT_KEY_SIZE);
  entropy_source.GetSecureRandom(&fek_[0], fek_.size());

  fek_sig_.resize(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE);
  entropy_source.GetSecureRandom(&fek_sig_[0], fek_sig_.size());

  fek_salt_.resize(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  entropy_source.GetSecureRandom(&fek_salt_[0], fek_salt_.size());

  fnek_.resize(CRYPTOHOME_DEFAULT_KEY_SIZE);
  entropy_source.GetSecureRandom(&fnek_[0], fnek_.size());

  fnek_sig_.resize(CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE);
  entropy_source.GetSecureRandom(&fnek_sig_[0], fnek_sig_.size());

  fnek_salt_.resize(CRYPTOHOME_DEFAULT_KEY_SALT_SIZE);
  entropy_source.GetSecureRandom(&fnek_salt_[0], fnek_salt_.size());
}

const SecureBlob& VaultKeyset::FEK() const {
  return fek_;
}

const SecureBlob& VaultKeyset::FEK_SIG() const {
  return fek_sig_;
}

const SecureBlob& VaultKeyset::FEK_SALT() const {
  return fek_salt_;
}

const SecureBlob& VaultKeyset::FNEK() const {
  return fnek_;
}

const SecureBlob& VaultKeyset::FNEK_SIG() const {
  return fnek_sig_;
}

const SecureBlob& VaultKeyset::FNEK_SALT() const {
  return fnek_salt_;
}

unsigned int VaultKeyset::SerializedSize() {
  return sizeof(VaultKeysetHeader) + sizeof(VaultKeysetKeys);
}

}  // cryptohome
