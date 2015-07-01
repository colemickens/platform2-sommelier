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

// From definition of TPMA_STARTUP_CLEAR.
const trunks::TPMA_STARTUP_CLEAR kPlatformHierarchyMask = 1U;

// A test fixture for TpmState tests.
class TpmStateTest : public testing::Test {
 public:
  TpmStateTest() {}
  ~TpmStateTest() override {}

  void SetUp() override {
    factory_.set_tpm(&mock_tpm_);
    permanent_data_ = GetValidCapabilityData(TPM_PT_PERMANENT, 0);
    startup_clear_data_ = GetValidCapabilityData(TPM_PT_STARTUP_CLEAR, 0);
    rsa_data_ = GetValidAlgorithmData(TPM_ALG_RSA, 0);
    ecc_data_ = GetValidAlgorithmData(TPM_ALG_ECC, 0);
    EXPECT_CALL(mock_tpm_, GetCapabilitySync(TPM_CAP_TPM_PROPERTIES,
                                             TPM_PT_PERMANENT, 1, _, _, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &TpmStateTest::GetLivePermanent)));
    EXPECT_CALL(mock_tpm_, GetCapabilitySync(TPM_CAP_TPM_PROPERTIES,
                                             TPM_PT_STARTUP_CLEAR, 1, _, _, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &TpmStateTest::GetLiveStartupClear)));
    EXPECT_CALL(mock_tpm_, GetCapabilitySync(TPM_CAP_ALGS,
                                             TPM_ALG_RSA, 1, _, _, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &TpmStateTest::GetLiveRSA)));
    EXPECT_CALL(mock_tpm_, GetCapabilitySync(TPM_CAP_ALGS,
                                             TPM_ALG_ECC, 1, _, _, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &TpmStateTest::GetLiveECC)));
  }

  TPM_RC GetLivePermanent(TPMS_CAPABILITY_DATA* capability_data) {
    *capability_data = permanent_data_;
    return TPM_RC_SUCCESS;
  }

  TPM_RC GetLiveStartupClear(TPMS_CAPABILITY_DATA* capability_data) {
    *capability_data = startup_clear_data_;
    return TPM_RC_SUCCESS;
  }

  TPM_RC GetLiveRSA(TPMS_CAPABILITY_DATA* capability_data) {
    *capability_data = rsa_data_;
    return TPM_RC_SUCCESS;
  }

  TPM_RC GetLiveECC(TPMS_CAPABILITY_DATA* capability_data) {
    *capability_data = ecc_data_;
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

  TPMS_CAPABILITY_DATA GetValidAlgorithmData(TPM_ALG_ID alg_id, UINT32 value) {
    TPMS_CAPABILITY_DATA data;
    memset(&data, 0, sizeof(TPMS_CAPABILITY_DATA));
    data.capability = TPM_CAP_ALGS;
    data.data.tpm_properties.count = 1;
    data.data.algorithms.alg_properties[0].alg = alg_id;
    data.data.algorithms.alg_properties[0].alg_properties = value;
    return data;
  }

  TrunksFactoryForTest factory_;
  NiceMock<MockTpm> mock_tpm_;
  TPMS_CAPABILITY_DATA permanent_data_;
  TPMS_CAPABILITY_DATA startup_clear_data_;
  TPMS_CAPABILITY_DATA rsa_data_;
  TPMS_CAPABILITY_DATA ecc_data_;
};

TEST(TpmState_DeathTest, NotInitialized) {
  TrunksFactoryForTest factory;
  TpmStateImpl tpm_state(factory);
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsOwnerPasswordSet(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsEndorsementPasswordSet(),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsLockoutPasswordSet(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsOwned(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsInLockout(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsPlatformHierarchyEnabled(),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsStorageHierarchyEnabled(),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsEndorsementHierarchyEnabled(),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsEnabled(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.WasShutdownOrderly(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsRSASupported(), "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(tpm_state.IsECCSupported(), "Check failed");
}

TEST_F(TpmStateTest, FlagsClear) {
  TpmStateImpl tpm_state(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, tpm_state.Initialize());
  EXPECT_FALSE(tpm_state.IsOwnerPasswordSet());
  EXPECT_FALSE(tpm_state.IsEndorsementPasswordSet());
  EXPECT_FALSE(tpm_state.IsLockoutPasswordSet());
  EXPECT_FALSE(tpm_state.IsInLockout());
  EXPECT_FALSE(tpm_state.IsOwned());
  EXPECT_FALSE(tpm_state.IsPlatformHierarchyEnabled());
  EXPECT_FALSE(tpm_state.IsStorageHierarchyEnabled());
  EXPECT_FALSE(tpm_state.IsEndorsementHierarchyEnabled());
  EXPECT_FALSE(tpm_state.IsEnabled());
  EXPECT_FALSE(tpm_state.WasShutdownOrderly());
  EXPECT_FALSE(tpm_state.IsRSASupported());
  EXPECT_FALSE(tpm_state.IsECCSupported());
}

TEST_F(TpmStateTest, FlagsSet) {
  permanent_data_.data.tpm_properties.tpm_property[0].value = ~0U;
  startup_clear_data_.data.tpm_properties.tpm_property[0].value = ~0U;
  rsa_data_.data.algorithms.alg_properties[0].alg_properties = ~0U;
  ecc_data_.data.algorithms.alg_properties[0].alg_properties = ~0U;
  TpmStateImpl tpm_state(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, tpm_state.Initialize());
  EXPECT_TRUE(tpm_state.IsOwnerPasswordSet());
  EXPECT_TRUE(tpm_state.IsEndorsementPasswordSet());
  EXPECT_TRUE(tpm_state.IsLockoutPasswordSet());
  EXPECT_TRUE(tpm_state.IsOwned());
  EXPECT_TRUE(tpm_state.IsInLockout());
  EXPECT_TRUE(tpm_state.IsPlatformHierarchyEnabled());
  EXPECT_TRUE(tpm_state.IsStorageHierarchyEnabled());
  EXPECT_TRUE(tpm_state.IsEndorsementHierarchyEnabled());
  EXPECT_FALSE(tpm_state.IsEnabled());
  EXPECT_TRUE(tpm_state.WasShutdownOrderly());
  EXPECT_TRUE(tpm_state.IsRSASupported());
  EXPECT_TRUE(tpm_state.IsECCSupported());
}

TEST_F(TpmStateTest, EnabledTpm) {
  startup_clear_data_.data.tpm_properties.tpm_property[0].value =
      ~kPlatformHierarchyMask;
  TpmStateImpl tpm_state(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, tpm_state.Initialize());
  EXPECT_FALSE(tpm_state.IsPlatformHierarchyEnabled());
  EXPECT_TRUE(tpm_state.IsStorageHierarchyEnabled());
  EXPECT_TRUE(tpm_state.IsEndorsementHierarchyEnabled());
  EXPECT_TRUE(tpm_state.IsEnabled());
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

TEST_F(TpmStateTest, BadResponseRSAAlgCapabilityType) {
  rsa_data_.capability = 0xFFFFF;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

TEST_F(TpmStateTest, BadResponseECCAlgCapabilityType) {
  ecc_data_.capability = 0xFFFFF;
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

TEST_F(TpmStateTest, BadResponseRSAAlgPropertyCount) {
  rsa_data_.data.algorithms.count = 0;
  TpmStateImpl tpm_state(factory_);
  EXPECT_NE(TPM_RC_SUCCESS, tpm_state.Initialize());
}

TEST_F(TpmStateTest, BadResponseECCAlgPropertyCount) {
  ecc_data_.data.algorithms.count = 0;
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
