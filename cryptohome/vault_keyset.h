// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VAULT_KEYSET_H_
#define VAULT_KEYSET_H_

#include <base/basictypes.h>
#include <chromeos/secure_blob.h>

#include "cryptohome_common.h"
#include "vault_keyset.pb.h"

namespace cryptohome {

class Crypto;
class Platform;

// VaultKeyset holds the File Encryption Key (FEK) and File Name Encryption Key
// (FNEK) and their corresponding signatures.
class VaultKeyset {
 public:
  VaultKeyset();
  virtual ~VaultKeyset();

  // Does not take ownership of platform and crypto. The objects pointed to by
  // them must outlive this object.
  virtual void Initialize(Platform* platform, Crypto* crypto);

  virtual void FromVaultKeyset(const VaultKeyset& vault_keyset);
  virtual void FromKeys(const VaultKeysetKeys& keys);
  virtual bool FromKeysBlob(const chromeos::SecureBlob& keys_blob);
  virtual bool ToKeys(VaultKeysetKeys* keys) const;
  virtual bool ToKeysBlob(chromeos::SecureBlob* keys_blob) const;

  virtual void CreateRandom();

  virtual const chromeos::SecureBlob& FEK() const;
  virtual const chromeos::SecureBlob& FEK_SIG() const;
  virtual const chromeos::SecureBlob& FEK_SALT() const;
  virtual const chromeos::SecureBlob& FNEK() const;
  virtual const chromeos::SecureBlob& FNEK_SIG() const;
  virtual const chromeos::SecureBlob& FNEK_SALT() const;

  virtual bool Load(const std::string& filename);
  // Load must be called first.
  virtual bool Decrypt(const chromeos::SecureBlob& key);
  // Encrypt must be called first.
  virtual bool Save(const std::string& filename);
  virtual bool Encrypt(const chromeos::SecureBlob& key);
  virtual const SerializedVaultKeyset& serialized() const {
    return serialized_;
  }
  virtual SerializedVaultKeyset* mutable_serialized() {
    return &serialized_;
  }

 private:
  chromeos::SecureBlob fek_;
  chromeos::SecureBlob fek_sig_;
  chromeos::SecureBlob fek_salt_;
  chromeos::SecureBlob fnek_;
  chromeos::SecureBlob fnek_sig_;
  chromeos::SecureBlob fnek_salt_;

  Platform* platform_;
  Crypto* crypto_;

  SerializedVaultKeyset serialized_;
  bool loaded_;
  bool encrypted_;

  DISALLOW_COPY_AND_ASSIGN(VaultKeyset);
};

}  // namespace cryptohome

#endif  // VAULT_KEYSET_H_
