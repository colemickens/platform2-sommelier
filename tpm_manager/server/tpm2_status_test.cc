//
// Copyright (C) 2015 The Android Open Source Project
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

#include "tpm_manager/server/tpm2_status_impl.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tpm_manager/server/dbus_service.h>
#include <trunks/mock_tpm_state.h>
#include <trunks/trunks_factory_for_test.h>

using testing::NiceMock;
using testing::Return;
using trunks::TPM_RC_FAILURE;
using trunks::TPM_RC_SUCCESS;

namespace tpm_manager {

class Tpm2StatusTest : public testing::Test {
 public:
  Tpm2StatusTest() = default;
  ~Tpm2StatusTest() override = default;

  void SetUp() override {
    factory_.set_tpm_state(&mock_tpm_state_);
    ownership_callback_ = base::Bind(
        &Tpm2StatusTest::OwnershipCallback, base::Unretained(this));
    tpm_status_.reset(new Tpm2StatusImpl(factory_, ownership_callback_));
  }

 protected:
  // Mock ownership taken callback.
  void OwnershipCallback() { ++ownership_call_count_; }

  NiceMock<trunks::MockTpmState> mock_tpm_state_;
  trunks::TrunksFactoryForTest factory_;
  std::unique_ptr<TpmStatus> tpm_status_;

  OwnershipTakenCallBack ownership_callback_;
  int ownership_call_count_ = 0;
};

TEST_F(Tpm2StatusTest, IsEnabledAlwaysSuccess) {
  EXPECT_CALL(mock_tpm_state_, Initialize()).Times(0);
  EXPECT_TRUE(tpm_status_->IsTpmEnabled());
}

TEST_F(Tpm2StatusTest, IsOwnedSuccess) {
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_state_, IsOwned()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_EQ(TpmStatus::kTpmOwned, tpm_status_->CheckAndNotifyIfTpmOwned());
  EXPECT_EQ(ownership_call_count_, 1);
}

TEST_F(Tpm2StatusTest, IsOwnedFailure) {
  EXPECT_CALL(mock_tpm_state_, IsOwned()).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_EQ(TpmStatus::kTpmUnowned, tpm_status_->CheckAndNotifyIfTpmOwned());
  EXPECT_EQ(ownership_call_count_, 0);
}

TEST_F(Tpm2StatusTest, IsOwnedRepeatedInitializationOnFalse) {
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .Times(2)
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_state_, IsOwned()).WillOnce(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_EQ(TpmStatus::kTpmUnowned, tpm_status_->CheckAndNotifyIfTpmOwned());
  EXPECT_EQ(ownership_call_count_, 0);
  EXPECT_CALL(mock_tpm_state_, IsOwned()).WillRepeatedly(Return(true));
  EXPECT_EQ(TpmStatus::kTpmOwned, tpm_status_->CheckAndNotifyIfTpmOwned());
  EXPECT_EQ(ownership_call_count_, 1);
}

TEST_F(Tpm2StatusTest, IsOwnedNoRepeatedInitializationOnTrue) {
  EXPECT_CALL(mock_tpm_state_, Initialize()).WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_state_, IsOwned()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_EQ(TpmStatus::kTpmOwned, tpm_status_->CheckAndNotifyIfTpmOwned());
  EXPECT_EQ(TpmStatus::kTpmOwned, tpm_status_->CheckAndNotifyIfTpmOwned());
  EXPECT_EQ(ownership_call_count_, 1);
}

TEST_F(Tpm2StatusTest, IsPreOwned) {
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_state_, IsOwned()).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_EQ(TpmStatus::kTpmPreOwned, tpm_status_->CheckAndNotifyIfTpmOwned());
  EXPECT_EQ(ownership_call_count_, 0);
}

TEST_F(Tpm2StatusTest, GetDictionaryAttackInfoInitializeFailure) {
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  uint32_t count;
  uint32_t threshold;
  bool lockout;
  uint32_t seconds_remaining;
  EXPECT_FALSE(tpm_status_->GetDictionaryAttackInfo(
      &count, &threshold, &lockout, &seconds_remaining));
}

TEST_F(Tpm2StatusTest, GetDictionaryAttackInfoForwarding) {
  uint32_t lockout_count = 3;
  uint32_t lockout_threshold = 16;
  bool is_locked = true;
  uint32_t lockout_interval = 3600;
  EXPECT_CALL(mock_tpm_state_, GetLockoutCounter())
      .WillRepeatedly(Return(lockout_count));
  EXPECT_CALL(mock_tpm_state_, GetLockoutThreshold())
      .WillRepeatedly(Return(lockout_threshold));
  EXPECT_CALL(mock_tpm_state_, IsInLockout()).WillRepeatedly(Return(is_locked));
  EXPECT_CALL(mock_tpm_state_, GetLockoutInterval())
      .WillRepeatedly(Return(lockout_interval));
  uint32_t count;
  uint32_t threshold;
  bool lockout;
  uint32_t seconds_remaining;
  EXPECT_TRUE(tpm_status_->GetDictionaryAttackInfo(&count, &threshold, &lockout,
                                                   &seconds_remaining));
  EXPECT_EQ(count, lockout_count);
  EXPECT_EQ(threshold, lockout_threshold);
  EXPECT_EQ(lockout, is_locked);
  EXPECT_EQ(seconds_remaining, lockout_count * lockout_interval);
}

TEST_F(Tpm2StatusTest, GetDictionaryAttackInfoAlwaysRefresh) {
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  uint32_t count;
  uint32_t threshold;
  bool lockout;
  uint32_t seconds_remaining;
  EXPECT_TRUE(tpm_status_->GetDictionaryAttackInfo(&count, &threshold, &lockout,
                                                   &seconds_remaining));
}

}  // namespace tpm_manager
