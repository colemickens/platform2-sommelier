// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "trunks/mock_tpm.h"
#include "trunks/mock_tpm_state.h"
#include "trunks/tpm_utility_impl.h"
#include "trunks/trunks_factory_for_test.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace trunks {

// A test fixture for TpmUtility tests.
class TpmUtilityTest : public testing::Test {
 public:
  TpmUtilityTest() {}
  virtual ~TpmUtilityTest() {}
  void SetUp() {
    factory_.set_tpm_state(&mock_tpm_state_);
    factory_.set_tpm(&mock_tpm_);
  }
 protected:
  TrunksFactoryForTest factory_;
  NiceMock<MockTpmState> mock_tpm_state_;
  NiceMock<MockTpm> mock_tpm_;
};

TEST_F(TpmUtilityTest, InitializeTpmAlreadyInit) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.InitializeTpm());
}

TEST_F(TpmUtilityTest, InitializeTpmSuccess) {
  TpmUtilityImpl utility(factory_);
  // Setup a hierarchy that needs to be disabled.
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
      .WillOnce(Return(true));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.InitializeTpm());
}

TEST_F(TpmUtilityTest, InitializeTpmBadAuth) {
  TpmUtilityImpl utility(factory_);
  // Setup a hierarchy that needs to be disabled.
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
      .WillOnce(Return(true));
  // Reject attempts to set platform auth.
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_PLATFORM, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.InitializeTpm());
}

TEST_F(TpmUtilityTest, InitializeTpmLockNVFails) {
  TpmUtilityImpl utility(factory_);
  // Setup a hierarchy that needs to be disabled.
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
      .WillOnce(Return(true));
  // Reject attempts to lock nv.
  EXPECT_CALL(mock_tpm_, NV_GlobalWriteLockSync(_, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.InitializeTpm());
}

TEST_F(TpmUtilityTest, InitializeTpmDisablePHFails) {
  TpmUtilityImpl utility(factory_);
  // Setup a hierarchy that needs to be disabled.
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
      .WillOnce(Return(true));
  // Reject attempts disable the platform hierarchy.
  EXPECT_CALL(mock_tpm_, HierarchyControlSync(_, _, TPM_RH_PLATFORM, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.InitializeTpm());
}

}  // namespace trunks
