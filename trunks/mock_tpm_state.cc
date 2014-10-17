// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/mock_tpm_state.h"

#include <gmock/gmock.h>

namespace trunks {

MockTpmState::MockTpmState() {
  ON_CALL(*this, IsOwnerPasswordSet()).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsEndorsementPasswordSet())
      .WillByDefault(testing::Return(true));
  ON_CALL(*this, IsLockoutPasswordSet()).WillByDefault(testing::Return(true));
  ON_CALL(*this, WasShutdownOrderly()).WillByDefault(testing::Return(true));
}
MockTpmState::~MockTpmState() {}

}  // namespace trunks
