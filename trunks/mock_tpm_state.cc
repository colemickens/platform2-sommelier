// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/mock_tpm_state.h"

#include <gmock/gmock.h>

using testing::Return;

namespace trunks {

MockTpmState::MockTpmState() {
  ON_CALL(*this, IsOwnerPasswordSet()).WillByDefault(Return(true));
  ON_CALL(*this, IsEndorsementPasswordSet()).WillByDefault(Return(true));
  ON_CALL(*this, IsLockoutPasswordSet()).WillByDefault(Return(true));
  ON_CALL(*this, WasShutdownOrderly()).WillByDefault(Return(true));
  ON_CALL(*this, IsRSASupported()).WillByDefault(Return(true));
  ON_CALL(*this, IsECCSupported()).WillByDefault(Return(true));
  ON_CALL(*this, IsPlatformHierarchyEnabled()).WillByDefault(Return(true));
}

MockTpmState::~MockTpmState() {}

}  // namespace trunks
