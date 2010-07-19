// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VAULT_KEYSET_H_
#define VAULT_KEYSET_H_

#include <base/basictypes.h>

#include "cryptohome_common.h"
#include "entropy_source.h"
#include "secure_blob.h"

namespace cryptohome {

// VaultKeyset holds the File Encryption Key (FEK) and File Name Encryption Key
// (FNEK) and their corresponding signatures.
class VaultKeyset {
 public:
  VaultKeyset();
  virtual ~VaultKeyset();

  void FromVaultKeyset(const VaultKeyset& vault_keyset);
  void FromKeys(const VaultKeysetKeys& keys);
  bool FromKeysBlob(const SecureBlob& keys_blob);
  bool ToKeys(VaultKeysetKeys* keys) const;
  bool ToKeysBlob(SecureBlob* keys_blob) const;

  void CreateRandom(const EntropySource& entropy_source);

  const SecureBlob& FEK() const;
  const SecureBlob& FEK_SIG() const;
  const SecureBlob& FEK_SALT() const;
  const SecureBlob& FNEK() const;
  const SecureBlob& FNEK_SIG() const;
  const SecureBlob& FNEK_SALT() const;

 private:

  SecureBlob fek_;
  SecureBlob fek_sig_;
  SecureBlob fek_salt_;
  SecureBlob fnek_;
  SecureBlob fnek_sig_;
  SecureBlob fnek_salt_;

  DISALLOW_COPY_AND_ASSIGN(VaultKeyset);
};

}  // namespace cryptohome

#endif  // VAULT_KEYSET_H_
