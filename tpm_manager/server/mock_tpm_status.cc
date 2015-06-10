// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/mock_tpm_status.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace tpm_manager {

bool GetDefaultDictionaryAttackInfo(int* counter,
                                    int* threshold,
                                    bool* lockout,
                                    int* seconds_remaining) {
  *counter = 0;
  *threshold = 10;
  *lockout = false;
  *seconds_remaining = 0;
  return true;
}

MockTpmStatus::MockTpmStatus() {
  ON_CALL(*this, IsTpmEnabled()).WillByDefault(Return(true));
  ON_CALL(*this, IsTpmOwned()).WillByDefault(Return(true));
  ON_CALL(*this, GetDictionaryAttackInfo(_, _, _, _))
      .WillByDefault(Invoke(GetDefaultDictionaryAttackInfo));
}
MockTpmStatus::~MockTpmStatus() {}

}  // namespace tpm_manager
