// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "trunks/mock_tpm.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_state_impl.h"
#include "trunks/trunks_factory_for_test.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::WithArgs;

namespace trunks {

// A test fixture for TpmState tests.
class TpmStateTest : public testing::Test {
 public:
  TpmStateTest() {}
  virtual ~TpmStateTest() {}

  void SetUp() {
    factory_.set_tpm(&mock_tpm_);
    permanent_data_ = GetValidCapabilityData(TPM_PT_PERMANENT, 0);
    startup_clear_data_ = GetValidCapabilityData(TPM_PT_STARTUP_CLEAR, 0);
    EXPECT_CALL(mock_tpm_, GetCapabilitySync(TPM_CAP_TPM_PROPERTIES,
                                             TPM_PT_PERMANENT, 1, _, _, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &TpmStateTest::GetLivePermanent)));
    EXPECT_CALL(mock_tpm_, GetCapabilitySync(TPM_CAP_TPM_PROPERTIES,
                                             TPM_PT_STARTUP_CLEAR, 1, _, _, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &TpmStateTest::GetLiveStartupClear)));
  }

  TPM_RC GetLivePermanent(TPMS_CAPABILITY_DATA* capability_data) {
    *capability_data = permanent_data_;
    return TPM_RC_SUCCESS;
  }

  TPM_RC GetLiveStartupClear(TPMS_CAPABILITY_DATA* capability_data) {
    *capability_data = startup_clear_data_;
    return TPM_RC_SUCCESS;
  }

 protected:
  TPMS_CAPABILITY_DATA GetValidCapabilityData(TPM_PT property, UINT32 value) {
    TPMS_CAPABILITY_DATA data;
    memset(&data, 0, sizeof(TPMS_CAPABILITY_DATA));
    data.capability = TPM_CAP_TPM_PROPERTIES;
    data.data.tpm_properties.count = 1;
    data.data.tpm_properties.tpm_property[0].property = property;
    data.data.tpm_properties.tpm_property[0].value = value;
    return data;
  }

  TrunksFactoryForTest factory_;
  NiceMock<MockTpm> mock_tpm_;
  TPMS_CAPABILITY_DATA permanent_data_;
  TPMS_CAPABILITY_DATA startup_clear_data_;
};

TEST(TpmState_DeathTest, NotInitialized) {
  TrunksFactoryForTest factory;
  TpmStateImpl tpm_state(factory);
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsOwnerPasswordSet(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsEndorsementPasswordSet(),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsLockoutPasswordSet(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsInLockout(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsPlatformHierarchyEnabled(),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.WasShutdownOrderly(), "Check failed");
}

TEST_F(TpmStateTest, FlagsClear) {
  TpmStateImpl tpm_state(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, tpm_state.Initialize());
  EXPECT_FALSE(tpm_state.IsOwnerPasswordSet());
  EXPECT_FALSE(tpm_state.IsEndorsementPasswordSet());
  EXPECT_FALSE(tpm_state.IsLockoutPasswordSet());
  EXPECT_FALSE(tpm_state.IsInLockout());
  EXPECT_FALSE(tpm_state.IsPlatformHierarchyEnabled());
  EXPECT_FALSE(tpm_state.WasShutdownOrderly());
}

TEST_F(TpmStateTest, FlagsSet) {
  permanent_data_.data.tpm_properties.tpm_property[0].value = ~0U;
  startup_clear_data_.data.tpm_properties.tpm_property[0].value = ~0U;
  TpmStateImpl tpm_state(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, tpm_state.Initialize());
  EXPECT_TRUE(tpm_state.IsOwnerPasswordSet());
  EXPECT_TRUE(tpm_state.IsEndorsementPasswordSet());
  EXPECT_TRUE(tpm_state.IsLockoutPasswordSet());
  EXPECT_TRUE(tpm_state.IsInLockout());
  EXPECT_TRUE(tpm_state.IsPlatformHierarchyEnabled());
  EXPECT_TRUE(tpm_state.WasShutdownOrderly());
}

TEST_F(TpmStateTest, BadResponsePermanentCapabilityType) {
  permanent_data_.capability = 0xFFFFF;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

TEST_F(TpmStateTest, BadResponseStartupClearCapabilityType) {
  startup_clear_data_.capability = 0xFFFFF;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

TEST_F(TpmStateTest, BadResponsePermanentPropertyCount) {
  permanent_data_.data.tpm_properties.count = 0;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

TEST_F(TpmStateTest, BadResponseStartupClearPropertyCount) {
  startup_clear_data_.data.tpm_properties.count = 0;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

TEST_F(TpmStateTest, BadResponsePermanentPropertyType) {
  permanent_data_.data.tpm_properties.tpm_property[0].property = 0xFFFFF;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

TEST_F(TpmStateTest, BadResponseStartupClearPropertyType) {
  startup_clear_data_.data.tpm_properties.tpm_property[0].property = 0xFFFFF;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

}  // namespace trunks
