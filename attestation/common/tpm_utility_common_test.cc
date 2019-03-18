//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "attestation/common/tpm_utility_common.h"

#if !USE_TPM2
#include "attestation/common/tpm_utility_v1.h"
#endif

#include <vector>

#include <base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tpm_manager-client/tpm_manager/dbus-constants.h>
#include <tpm_manager/client/mock_tpm_manager_utility.h>

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Types;

}  // namespace

namespace attestation {

template <typename TpmUtilityType>
class TpmUtilityCommonTest : public ::testing::Test {
 public:
  ~TpmUtilityCommonTest() override = default;
  void SetUp() override {
    mock_tpm_manager_utility_ = new tpm_manager::MockTpmManagerUtility();
    tpm_manager_utility_backup_.reset(mock_tpm_manager_utility_);
    tpm_utility_.tpm_manager_utility_.swap(tpm_manager_utility_backup_);
  }
  void TearDown() override {
    tpm_utility_.tpm_manager_utility_.swap(tpm_manager_utility_backup_);
  }

 protected:
  // Checks if GetTpmStatus sets up the private data member.
  void VerifyAgainstExpectedLocalData(const tpm_manager::LocalData local_data) {
    EXPECT_EQ(tpm_utility_.owner_password_, local_data.owner_password());
    EXPECT_EQ(tpm_utility_.endorsement_password_,
              local_data.endorsement_password());
    EXPECT_EQ(tpm_utility_.delegate_blob_, local_data.owner_delegate().blob());
    EXPECT_EQ(tpm_utility_.delegate_secret_,
              local_data.owner_delegate().secret());
  }
  TpmUtilityType tpm_utility_;
  tpm_manager::MockTpmManagerUtility* mock_tpm_manager_utility_{nullptr};
  std::unique_ptr<tpm_manager::TpmManagerUtility> tpm_manager_utility_backup_;
};

#if USE_TPM2
using TpmUtilitiesUnderTest = Types<>;
#else
using TpmUtilitiesUnderTest = Types<TpmUtilityV1>;
#endif

TYPED_TEST_CASE(TpmUtilityCommonTest, TpmUtilitiesUnderTest);

TYPED_TEST(TpmUtilityCommonTest, HasTpmManagerUtility) {
  EXPECT_THAT(this->tpm_manager_utility_backup_, NotNull());
}

TYPED_TEST(TpmUtilityCommonTest, IsTpmReady) {
  EXPECT_CALL(*this->mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(Return(false))
      .WillOnce(
          DoAll(SetArgPointee<0>(false), SetArgPointee<1>(false), Return(true)))
      .WillOnce(
          DoAll(SetArgPointee<0>(true), SetArgPointee<1>(false), Return(true)));
  EXPECT_FALSE(this->tpm_utility_.IsTpmReady());
  EXPECT_FALSE(this->tpm_utility_.IsTpmReady());
  EXPECT_FALSE(this->tpm_utility_.IsTpmReady());

  EXPECT_CALL(*this->mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(
          DoAll(SetArgPointee<0>(true), SetArgPointee<1>(true), Return(true)));
  EXPECT_TRUE(this->tpm_utility_.IsTpmReady());
}

TYPED_TEST(TpmUtilityCommonTest, IsTpmReadyCallsCacheTpmState) {
  tpm_manager::LocalData expected_local_data;
  expected_local_data.set_owner_password("Uvuvwevwevwe");
  expected_local_data.set_endorsement_password("Onyetenyevwe");
  expected_local_data.mutable_owner_delegate()->set_blob("Ugwemuhwem");
  expected_local_data.mutable_owner_delegate()->set_secret("Osas");
  EXPECT_CALL(*this->mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(expected_local_data), Return(true)));
  this->tpm_utility_.IsTpmReady();
  this->VerifyAgainstExpectedLocalData(expected_local_data);
}

TYPED_TEST(TpmUtilityCommonTest, RemoveOwnerDependency) {
  EXPECT_CALL(
      *this->mock_tpm_manager_utility_,
      RemoveOwnerDependency(tpm_manager::kTpmOwnerDependency_Attestation))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_FALSE(this->tpm_utility_.RemoveOwnerDependency());
  EXPECT_TRUE(this->tpm_utility_.RemoveOwnerDependency());
}

}  // namespace attestation
