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

  MOCK_METHOD1(Init, void(OwnershipCallback));
  MOCK_METHOD1(SetupTpm, bool(bool));
  MOCK_METHOD1(RemoveTpmOwnerDependency,
               void(TpmPersistentState::TpmOwnerDependency));
  MOCK_METHOD0(HasCryptohomeKey, bool());
  MOCK_METHOD0(IsTpmReady, bool());
  MOCK_METHOD0(IsTpmEnabled, bool());
  MOCK_METHOD0(IsTpmOwned, bool());
  MOCK_METHOD1(GetTpmPassword, bool(brillo::SecureBlob*));
  MOCK_METHOD0(ShallInitialize, bool());
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_TPM_INIT_H_
