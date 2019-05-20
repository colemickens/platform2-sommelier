// Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/tpm_new_impl.h"

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tpm_manager/client/mock_tpm_manager_utility.h>
#include <tpm_manager/proto_bindings/tpm_manager.pb.h>

namespace {

using ::testing::_;
using ::testing::ByRef;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Test;

using brillo::Blob;
using brillo::BlobToString;
using brillo::SecureBlob;
using tpm_manager::LocalData;
using tpm_manager::MockTpmManagerUtility;

}  // namespace

namespace cryptohome {

class TpmNewImplTest : public Test {
 public:
  TpmNewImplTest() = default;
  ~TpmNewImplTest() = default;

 protected:
  NiceMock<MockTpmManagerUtility> mock_tpm_manager_utility_;
  TpmNewImpl tpm_{&mock_tpm_manager_utility_};
  Tpm* GetTpm() { return &tpm_; }
};

TEST_F(TpmNewImplTest, TakeOwnership) {
  EXPECT_CALL(mock_tpm_manager_utility_, TakeOwnership())
      .WillOnce(Return(false));
  EXPECT_FALSE(GetTpm()->TakeOwnership(0, SecureBlob{}));
  EXPECT_CALL(mock_tpm_manager_utility_, TakeOwnership())
      .WillOnce(Return(true));
  EXPECT_TRUE(GetTpm()->TakeOwnership(0, SecureBlob{}));

  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(true), Return(true)));
  EXPECT_CALL(mock_tpm_manager_utility_, TakeOwnership()).Times(0);
  EXPECT_TRUE(GetTpm()->TakeOwnership(0, SecureBlob{}));
}

TEST_F(TpmNewImplTest, EnabledAndOwned) {
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(GetTpm()->IsEnabled());
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(GetTpm()->IsOwned());

  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)));
  EXPECT_FALSE(GetTpm()->IsEnabled());
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(false), Return(true)));
  EXPECT_FALSE(GetTpm()->IsOwned());

  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));
  EXPECT_TRUE(GetTpm()->IsEnabled());
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(true), Return(true)));
  EXPECT_TRUE(GetTpm()->IsOwned());

  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _)).Times(0);
  EXPECT_TRUE(GetTpm()->IsEnabled());
  EXPECT_TRUE(GetTpm()->IsOwned());
}

TEST_F(TpmNewImplTest, GetOwnerPassword) {
  SecureBlob result_owner_password;
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(GetTpm()->GetOwnerPassword(&result_owner_password));
  LocalData expected_local_data;
  expected_local_data.set_owner_password("owner password");
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(true), SetArgPointee<1>(true),
                      SetArgPointee<2>(expected_local_data), Return(true)));
  EXPECT_TRUE(GetTpm()->GetOwnerPassword(&result_owner_password));
  EXPECT_EQ(result_owner_password.to_string(),
            expected_local_data.owner_password());

  result_owner_password.clear();
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _)).Times(0);
  EXPECT_TRUE(GetTpm()->GetOwnerPassword(&result_owner_password));
  EXPECT_EQ(result_owner_password.to_string(),
            expected_local_data.owner_password());
}

TEST_F(TpmNewImplTest, GetOwnerPasswordEmpty) {
  SecureBlob result_owner_password;
  EXPECT_FALSE(GetTpm()->GetOwnerPassword(&result_owner_password));
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(true), SetArgPointee<1>(true),
                      SetArgPointee<2>(LocalData{}), Return(true)));
  EXPECT_FALSE(GetTpm()->GetOwnerPassword(&result_owner_password));
}

TEST_F(TpmNewImplTest, GetDelegate) {
  Blob result_blob;
  Blob result_secret;
  bool result_has_reset_lock_permissions = false;
  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(GetTpm()->GetDelegate(&result_blob, &result_secret,
                                     &result_has_reset_lock_permissions));
  LocalData expected_local_data;

  EXPECT_CALL(mock_tpm_manager_utility_, GetTpmStatus(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(true), SetArgPointee<1>(true),
                            SetArgPointee<2>(ByRef(expected_local_data)),
                            Return(true)));
  EXPECT_FALSE(GetTpm()->GetDelegate(&result_blob, &result_secret,
                                     &result_has_reset_lock_permissions));

  expected_local_data.mutable_owner_delegate()->set_blob("blob");
  expected_local_data.mutable_owner_delegate()->set_secret("secret");
  expected_local_data.mutable_owner_delegate()->set_has_reset_lock_permissions(
      true);
  EXPECT_TRUE(GetTpm()->GetDelegate(&result_blob, &result_secret,
                                    &result_has_reset_lock_permissions));
  EXPECT_THAT(result_blob,
              ElementsAreArray(expected_local_data.owner_delegate().blob()));
  EXPECT_THAT(result_secret,
              ElementsAreArray(expected_local_data.owner_delegate().secret()));
  EXPECT_TRUE(result_has_reset_lock_permissions);
}

TEST_F(TpmNewImplTest, GetDictionaryAttackInfo) {
  int result_counter = 0;
  int result_threshold = 0;
  bool result_lockout = false;
  int result_seconds_remaining = 0;
  EXPECT_CALL(mock_tpm_manager_utility_, GetDictionaryAttackInfo(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(GetTpm()->GetDictionaryAttackInfo(
      &result_counter, &result_threshold, &result_lockout,
      &result_seconds_remaining));

  EXPECT_CALL(mock_tpm_manager_utility_, GetDictionaryAttackInfo(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(123), SetArgPointee<1>(456),
                      SetArgPointee<2>(true), SetArgPointee<3>(789),
                      Return(true)));
  EXPECT_TRUE(GetTpm()->GetDictionaryAttackInfo(
      &result_counter, &result_threshold, &result_lockout,
      &result_seconds_remaining));
  EXPECT_EQ(result_counter, 123);
  EXPECT_EQ(result_threshold, 456);
  EXPECT_TRUE(result_lockout);
  EXPECT_EQ(result_seconds_remaining, 789);
}

TEST_F(TpmNewImplTest, ResetDictionaryAttackMitigation) {
  EXPECT_CALL(mock_tpm_manager_utility_, ResetDictionaryAttackLock())
      .WillOnce(Return(false));
  EXPECT_FALSE(GetTpm()->ResetDictionaryAttackMitigation(Blob{}, Blob{}));
  EXPECT_CALL(mock_tpm_manager_utility_, ResetDictionaryAttackLock())
      .WillOnce(Return(true));
  EXPECT_TRUE(GetTpm()->ResetDictionaryAttackMitigation(Blob{}, Blob{}));
}

TEST_F(TpmNewImplTest, BadTpmManagerUtility) {
  EXPECT_CALL(mock_tpm_manager_utility_, Initialize())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(GetTpm()->TakeOwnership(0, SecureBlob{}));
  SecureBlob result_owner_password;
  EXPECT_FALSE(GetTpm()->GetOwnerPassword(&result_owner_password));
  EXPECT_FALSE(GetTpm()->IsEnabled());
  EXPECT_FALSE(GetTpm()->IsOwned());
  EXPECT_FALSE(GetTpm()->ResetDictionaryAttackMitigation(Blob{}, Blob{}));
  int result_counter;
  int result_threshold;
  bool result_lockout;
  int result_seconds_remaining;
  EXPECT_FALSE(GetTpm()->GetDictionaryAttackInfo(
      &result_counter, &result_threshold, &result_lockout,
      &result_seconds_remaining));
  Blob result_blob;
  Blob result_secret;
  bool result_has_reset_lock_permissions;
  EXPECT_FALSE(GetTpm()->GetDelegate(&result_blob, &result_secret,
                                     &result_has_reset_lock_permissions));
}

}  // namespace cryptohome
