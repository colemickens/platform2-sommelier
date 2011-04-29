// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/wipe_mitigator.h"

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>

#include "chromeos/dbus/service_constants.h"
#include "login_manager/mock_owner_key.h"
#include "login_manager/mock_system_utils.h"

using ::testing::Return;
using ::testing::StrEq;

namespace login_manager {

class WipeMitigatorTest : public ::testing::Test {
 public:
  WipeMitigatorTest() {}
  virtual ~WipeMitigatorTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WipeMitigatorTest);
};

TEST_F(WipeMitigatorTest, Mitigate) {
  MockSystemUtils* utils = new MockSystemUtils;
  EXPECT_CALL(*utils, TouchResetFile())
      .WillOnce(Return(true));
  EXPECT_CALL(*utils,
              AppendToClobberLog(StrEq(OwnerKeyLossMitigator::kMitigateMsg)))
      .Times(1);
  EXPECT_CALL(*utils,
              SendSignalToPowerManager(power_manager::kRequestRestartSignal))
      .Times(1);

  MockOwnerKey key;
  WipeMitigator mitigator(utils);
  EXPECT_FALSE(mitigator.Mitigate(&key));
}

}  // namespace login_manager
