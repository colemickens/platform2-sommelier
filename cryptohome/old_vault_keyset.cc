// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <chromeos/utility.h>

#include "cryptohome_common.h"
#include "old_vault_keyset.h"

namespace cryptohome {

OldVaultKeyset::OldVaultKeyset()
    : major_version_(1),
      minor_version_(0) {
}

bool OldVaultKeyset::AssignBuffer(const SecureBlob& source) {
  if(source.size() < OldVaultKeyset::SerializedSize()) {
    LOG(ERROR) << "Input buffer is too small.";
    return false;
  }

  int offset = 0;
  OldVaultKeysetHeader header;
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
  FromKeys(keys);

  return true;
}

bool OldVaultKeyset::ToBuffer(SecureBlob* buffer) const {
  SecureBlob local_buffer(OldVaultKeyset::SerializedSize());
  unsigned char* data = static_cast<unsigned char*>(local_buffer.data());

  OldVaultKeysetHeader header;
  memcpy(header.signature, kVaultKeysetSignature, sizeof(header.signature));
  header.major_version = major_version_;
  header.minor_version = minor_version_;
  memcpy(data, &header, sizeof(header));

  VaultKeysetKeys keys;
  if (!ToKeys(&keys)) {
    return false;
  }
  memcpy(&data[sizeof(header)], &keys, sizeof(keys));
  chromeos::SecureMemset(&keys, 0, sizeof(keys));

  buffer->swap(local_buffer);
  return true;
}

unsigned int OldVaultKeyset::SerializedSize() {
  return sizeof(OldVaultKeysetHeader) + sizeof(VaultKeysetKeys);
}

}  // namespace cryptohome
