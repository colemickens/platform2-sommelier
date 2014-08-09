// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_HOMEDIRS_H_
#define CRYPTOHOME_MOCK_HOMEDIRS_H_

#include "cryptohome/homedirs.h"

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>

#include "cryptohome/credentials.h"

namespace cryptohome {
class VaultKeyset;

class MockHomeDirs : public HomeDirs {
 public:
  MockHomeDirs();
  virtual ~MockHomeDirs();

  MOCK_METHOD3(Init, bool(Platform*, Crypto*,
                          UserOldestActivityTimestampCache*));
  MOCK_METHOD0(FreeDiskSpace, bool());
  MOCK_METHOD1(AreCredentialsValid, bool(const Credentials&));
  MOCK_METHOD2(GetValidKeyset, bool(const Credentials&, VaultKeyset*));
  MOCK_METHOD1(Remove, bool(const std::string&));
  MOCK_METHOD2(Migrate, bool(const Credentials&, const chromeos::SecureBlob&));
  MOCK_CONST_METHOD1(Exists, bool(const Credentials&));
  MOCK_CONST_METHOD1(GetVaultKeyset, VaultKeyset*(const Credentials&));
  MOCK_CONST_METHOD2(GetVaultKeysets,
                     bool(const std::string&, std::vector<int>*));
  MOCK_METHOD5(AddKeyset, CryptohomeErrorCode(const Credentials&,
                                              const chromeos::SecureBlob&,
                                              const KeyData*,
                                              bool,
                                              int*));
  MOCK_METHOD2(RemoveKeyset, CryptohomeErrorCode(const Credentials&,
                                                 const KeyData&));
  MOCK_METHOD2(ForceRemoveKeyset, bool(const std::string&, int));
  MOCK_METHOD3(MoveKeyset, bool(const std::string&, int, int));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_HOMEDIRS_H_
