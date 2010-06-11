// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VAULT_KEYSET_H_
#define VAULT_KEYSET_H_

#include <base/basictypes.h>

#include "entropy_source.h"
#include "secure_blob.h"

namespace cryptohome {

const char kVaultKeysetSignature[] = "ch";

#define CRYPTOHOME_VAULT_KEYSET_VERSION_MAJOR 1
#define CRYPTOHOME_VAULT_KEYSET_VERSION_MINOR 0

// VaultKeyset holds the File Encryption Key (FEK) and File Name Encryption Key
// (FNEK) and their corresponding signatures.
class VaultKeyset {
 public:
  VaultKeyset();

  bool AssignBuffer(const SecureBlob& buffer);
  bool ToBuffer(SecureBlob* buffer) const;

  void CreateRandom(const EntropySource& entropy_source);

  const SecureBlob& FEK() const;
  const SecureBlob& FEK_SIG() const;
  const SecureBlob& FEK_SALT() const;
  const SecureBlob& FNEK() const;
  const SecureBlob& FNEK_SIG() const;
  const SecureBlob& FNEK_SALT() const;

  static unsigned int SerializedSize();

 private:

  SecureBlob fek_;
  SecureBlob fek_sig_;
  SecureBlob fek_salt_;
  SecureBlob fnek_;
  SecureBlob fnek_sig_;
  SecureBlob fnek_salt_;
  unsigned short major_version_;
  unsigned short minor_version_;

  DISALLOW_COPY_AND_ASSIGN(VaultKeyset);
};

}  // namespace cryptohome

#endif  // VAULT_KEYSET_H_
