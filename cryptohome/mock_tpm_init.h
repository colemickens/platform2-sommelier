// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_TPM_INIT_H_
#define CRYPTOHOME_MOCK_TPM_INIT_H_

#include "cryptohome/tpm_init.h"

#include <gmock/gmock.h>

namespace cryptohome {

class MockTpmInit : public TpmInit {
 public:
  MockTpmInit();
  ~MockTpmInit();

  MOCK_METHOD(void, Init, (OwnershipCallback), (override));
  MOCK_METHOD(bool, SetupTpm, (bool), (override));
  MOCK_METHOD(void,
              RemoveTpmOwnerDependency,
              (TpmPersistentState::TpmOwnerDependency),
              (override));
  MOCK_METHOD(bool, HasCryptohomeKey, (), (override));
  MOCK_METHOD(bool, IsTpmReady, (), (override));
  MOCK_METHOD(bool, IsTpmEnabled, (), (override));
  MOCK_METHOD(bool, IsTpmOwned, (), (override));
  MOCK_METHOD(bool, GetTpmPassword, (brillo::SecureBlob*), (override));
  MOCK_METHOD(bool, ShallInitialize, (), (override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_TPM_INIT_H_
