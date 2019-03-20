// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/client/tpm_manager_utility.h"

#include <gtest/gtest.h>

#include "tpm_manager/common/mock_tpm_nvram_interface.h"
#include "tpm_manager/common/mock_tpm_ownership_interface.h"

namespace {

using ::testing::Test;

}  // namespace

namespace tpm_manager {

class TpmManagerUtilityTest : public Test {
 protected:
  // Uses contructor and SetUp at the same time so we can take advantage of
  // both sytles of initialization.
  TpmManagerUtilityTest()
      : tpm_manager_utility_(&mock_tpm_owner_, &mock_tpm_nvram_) {}
  void SetUp() override { ASSERT_TRUE(tpm_manager_utility_.Initialize()); }
  tpm_manager::MockTpmOwnershipInterface mock_tpm_owner_;
  tpm_manager::MockTpmNvramInterface mock_tpm_nvram_;
  TpmManagerUtility tpm_manager_utility_;
};

// An intial test to validate the constructor and SetUp() logic.
// TODO(cylai): remove this item along with the first test item added.
TEST_F(TpmManagerUtilityTest, FixtureSetUp) {
  EXPECT_TRUE(true);
}

}  // namespace tpm_manager
