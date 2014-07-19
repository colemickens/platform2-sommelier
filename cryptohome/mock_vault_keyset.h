// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_VAULT_KEYSET_H_
#define CRYPTOHOME_MOCK_VAULT_KEYSET_H_

#include "cryptohome/vault_keyset.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>

namespace cryptohome {
class Platform;
class Crypto;

class MockVaultKeyset : public VaultKeyset {
 public:
  MockVaultKeyset();
  virtual ~MockVaultKeyset();

  MOCK_METHOD2(Initialize, void(Platform*, Crypto*));
  MOCK_METHOD1(FromVaultKeyset, void(const VaultKeyset&));
  MOCK_METHOD1(FromKeys, void(const VaultKeysetKeys&));
  MOCK_METHOD1(FromKeysBlob, bool(const chromeos::SecureBlob&));
  MOCK_CONST_METHOD1(ToKeys, bool(VaultKeysetKeys*));  // NOLINT 'unnamed' param
  MOCK_CONST_METHOD1(ToKeysBlob, bool(chromeos::SecureBlob*));

  MOCK_METHOD0(CreateRandom, void(void));

  MOCK_CONST_METHOD0(FEK, const chromeos::SecureBlob&(void));
  MOCK_CONST_METHOD0(FEK_SIG, const chromeos::SecureBlob&(void));
  MOCK_CONST_METHOD0(FEK_SALT, const chromeos::SecureBlob&(void));
  MOCK_CONST_METHOD0(FNEK, const chromeos::SecureBlob&(void));
  MOCK_CONST_METHOD0(FNEK_SIG, const chromeos::SecureBlob&(void));
  MOCK_CONST_METHOD0(FNEK_SALT, const chromeos::SecureBlob&(void));

  MOCK_METHOD1(Load, bool(const std::string&));
  MOCK_METHOD1(Decrypt, bool(const chromeos::SecureBlob&));
  MOCK_METHOD1(Save, bool(const std::string&));
  MOCK_METHOD1(Encrypt, bool(const chromeos::SecureBlob&));
  MOCK_CONST_METHOD0(serialized, const SerializedVaultKeyset&(void));
  MOCK_METHOD0(mutable_serialized, SerializedVaultKeyset*(void));
  MOCK_CONST_METHOD0(source_file, const std::string&(void));
  MOCK_METHOD1(set_legacy_index, void(int));
  MOCK_CONST_METHOD0(legacy_index, const int(void));

 private:
  SerializedVaultKeyset stub_serialized_;
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_VAULT_KEYSET_H_
