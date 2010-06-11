// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <chromeos/utility.h>

#include "cryptohome_common.h"
#include "vault_keyset.h"

namespace cryptohome {

VaultKeyset::VaultKeyset()
    : major_version_(CRYPTOHOME_VAULT_KEYSET_VERSION_MAJOR),
      minor_version_(CRYPTOHOME_VAULT_KEYSET_VERSION_MINOR) {
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
  chromeos::SecureMemset(&keys, 0, sizeof(keys));

  return true;
}

bool VaultKeyset::ToBuffer(SecureBlob* buffer) const {
  SecureBlob local_buffer(VaultKeyset::SerializedSize());
  unsigned char* data = static_cast<unsigned char*>(local_buffer.data());

  VaultKeysetHeader header;
  memcpy(header.signature, kVaultKeysetSignature, sizeof(header.signature));
  header.major_version = major_version_;
  header.minor_version = minor_version_;
  memcpy(data, &header, sizeof(header));

  VaultKeysetKeys keys;
  chromeos::SecureMemset(&keys, 0, sizeof(keys));
  if (fek_.size() != sizeof(keys.fek)) {
    return false;
  }
  memcpy(keys.fek, fek_.const_data(), sizeof(keys.fek));
  if (fek_sig_.size() != sizeof(keys.fek_sig)) {
    return false;
  }
  memcpy(keys.fek_sig, fek_sig_.const_data(), sizeof(keys.fek_sig));
  if (fek_salt_.size() != sizeof(keys.fek_salt)) {
    return false;
  }
  memcpy(keys.fek_salt, fek_salt_.const_data(), sizeof(keys.fek_salt));
  if (fnek_.size() != sizeof(keys.fnek)) {
    return false;
  }
  memcpy(keys.fnek, fnek_.const_data(), sizeof(keys.fnek));
  if (fnek_sig_.size() != sizeof(keys.fnek_sig)) {
    return false;
  }
  memcpy(keys.fnek_sig, fnek_sig_.const_data(), sizeof(keys.fnek_sig));
  if (fnek_salt_.size() != sizeof(keys.fnek_salt)) {
    return false;
  }
  memcpy(keys.fnek_salt, fnek_salt_.const_data(), sizeof(keys.fnek_salt));
  memcpy(&data[sizeof(header)], &keys, sizeof(keys));
  chromeos::SecureMemset(&keys, 0, sizeof(keys));

  buffer->swap(local_buffer);
  return true;
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

}  // namespace cryptohome
