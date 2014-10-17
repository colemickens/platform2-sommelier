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
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

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

TEST_F(TpmUtilityTest, StartupSuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Startup());
}

TEST_F(TpmUtilityTest, StartupAlreadyStarted) {
  EXPECT_CALL(mock_tpm_, StartupSync(_, _))
      .WillRepeatedly(Return(TPM_RC_INITIALIZE));
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Startup());
}

TEST_F(TpmUtilityTest, StartupFailure) {
  EXPECT_CALL(mock_tpm_, StartupSync(_, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_FAILURE, utility.Startup());
}

TEST_F(TpmUtilityTest, StartupSelfTestFailure) {
  EXPECT_CALL(mock_tpm_, SelfTestSync(_, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_FAILURE, utility.Startup());
}

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

TEST_F(TpmUtilityTest, InitializeTpmDisablePHFails) {
  TpmUtilityImpl utility(factory_);
  // Setup a hierarchy that needs to be disabled.
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
      .WillOnce(Return(true));
  // Reject attempts disable the platform hierarchy.
  EXPECT_CALL(mock_tpm_, HierarchyControlSync(_, _, TPM_RH_PLATFORM, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.InitializeTpm());
}

TEST_F(TpmUtilityTest, StirRandomSuccess) {
  TpmUtilityImpl utility(factory_);
  std::string entropy_data("large test data", 100);
  EXPECT_CALL(mock_tpm_, StirRandomSync(_, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.StirRandom(entropy_data));
}

TEST_F(TpmUtilityTest, StirRandomFails) {
  TpmUtilityImpl utility(factory_);
  std::string entropy_data("test data");
  EXPECT_CALL(mock_tpm_, StirRandomSync(_, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.StirRandom(entropy_data));
}

TEST_F(TpmUtilityTest, GenerateRandomSuccess) {
  TpmUtilityImpl utility(factory_);
  // This number is larger than the max bytes the GetRandom call can return.
  // Therefore we expect software to make multiple calls to fill this many
  // bytes.
  int num_bytes = 72;
  std::string random_data;
  TPM2B_DIGEST large_random;
  large_random.size = 32;
  TPM2B_DIGEST small_random;
  small_random.size = 8;

  EXPECT_CALL(mock_tpm_, GetRandomSync(_, _, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<1>(large_random),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, GetRandomSync(8, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(small_random),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.GenerateRandom(num_bytes, &random_data));
  EXPECT_EQ(num_bytes, random_data.size());
}

TEST_F(TpmUtilityTest, GenerateRandomFails) {
  TpmUtilityImpl utility(factory_);
  int num_bytes = 5;
  std::string random_data;
  EXPECT_CALL(mock_tpm_, GetRandomSync(_, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.GenerateRandom(num_bytes, &random_data));
}

TEST_F(TpmUtilityTest, ExtendPCRSuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ExtendPCR(1, "test digest"));
}

TEST_F(TpmUtilityTest, ExtendPCRFail) {
  TpmUtilityImpl utility(factory_);
  int pcr_index = 0;
  TPM_HANDLE pcr_handle = HR_PCR + pcr_index;
  EXPECT_CALL(mock_tpm_, PCR_ExtendSync(pcr_handle, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ExtendPCR(pcr_index, "test digest"));
}

TEST_F(TpmUtilityTest, ExtendPCRBadParam) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_FAILURE, utility.ExtendPCR(-1, "test digest"));
}

TEST_F(TpmUtilityTest, ReadPCRSuccess) {
  TpmUtilityImpl utility(factory_);
  // The |pcr_index| is chosen to match the structure for |pcr_select|.
  // If you change |pcr_index|, remember to change |pcr_select|.
  int pcr_index = 1;
  std::string pcr_value;
  TPML_PCR_SELECTION pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].sizeof_select = 1;
  pcr_select.pcr_selections[0].pcr_select[0] = 2;
  TPML_DIGEST pcr_values;
  pcr_values.count = 1;
  EXPECT_CALL(mock_tpm_, PCR_ReadSync(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(pcr_select),
                      SetArgPointee<3>(pcr_values),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ReadPCR(pcr_index, &pcr_value));
}

TEST_F(TpmUtilityTest, ReadPCRFail) {
  TpmUtilityImpl utility(factory_);
  std::string pcr_value;
  EXPECT_CALL(mock_tpm_, PCR_ReadSync(_, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ReadPCR(1, &pcr_value));
}

TEST_F(TpmUtilityTest, ReadPCRBadReturn) {
  TpmUtilityImpl utility(factory_);
  std::string pcr_value;
  EXPECT_CALL(mock_tpm_, PCR_ReadSync(_, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ReadPCR(1, &pcr_value));
}

}  // namespace trunks
