// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_MOCK_TPM_STATE_H_
#define TRUNKS_MOCK_TPM_STATE_H_

#include "trunks/tpm_state.h"

#include <gmock/gmock.h>

namespace trunks {

class MockTpmState : public TpmState {
 public:
  MockTpmState();
  virtual ~MockTpmState();

  MOCK_METHOD0(Initialize, TPM_RC());
  MOCK_METHOD0(IsInLockout, bool());
  MOCK_METHOD0(IsPlatformHierarchyEnabled, bool());
  MOCK_METHOD0(WasShutdownOrderly, bool());
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_TPM_STATE_H_
