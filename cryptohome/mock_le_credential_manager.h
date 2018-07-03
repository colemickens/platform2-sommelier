// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_LE_CREDENTIAL_MANAGER_H_
#define CRYPTOHOME_MOCK_LE_CREDENTIAL_MANAGER_H_

#include "cryptohome/le_credential_manager.h"

#include <string>

#include <base/files/file_path.h>
#include <gmock/gmock.h>

namespace cryptohome {

class MockLECredentialManager : public LECredentialManager {
 public:
  MockLECredentialManager(LECredentialBackend* le_backend,
                          const base::FilePath& le_basedir);
  ~MockLECredentialManager();
  MOCK_METHOD4(CheckCredential, LECredError(const uint64_t&,
      const brillo::SecureBlob&, brillo::SecureBlob*, brillo::SecureBlob*));
  MOCK_METHOD6(InsertCredential, LECredError(const brillo::SecureBlob&,
      const brillo::SecureBlob&, const brillo::SecureBlob&,
      const DelaySchedule&, const ValidPcrCriteria&, uint64_t*));
  MOCK_METHOD1(RemoveCredential, LECredError(const uint64_t&));
  MOCK_METHOD1(NeedsPcrBinding, bool(const uint64_t&));
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_LE_CREDENTIAL_MANAGER_H_
