// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for Tpm2Impl.

#include "cryptohome/tpm2_impl.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <trunks/mock_hmac_session.h>
#include <trunks/mock_tpm.h>
#include <trunks/mock_tpm_state.h>
#include <trunks/mock_tpm_utility.h>
#include <trunks/tpm_constants.h>
#include <trunks/trunks_factory.h>
#include <trunks/trunks_factory_for_test.h>

using chromeos::SecureBlob;
using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using trunks::TPM_RC_FAILURE;
using trunks::TPM_RC_SUCCESS;
using trunks::TrunksFactory;

namespace cryptohome {

class Tpm2Test : public testing::Test {
 public:
  Tpm2Test() {}
  ~Tpm2Test() override {}

  void SetUp() override {
    factory_.set_tpm(&mock_tpm_);
    factory_.set_tpm_state(&mock_tpm_state_);
    factory_.set_tpm_utility(&mock_tpm_utility_);
    factory_.set_hmac_session(&mock_hmac_session_);
    tpm_ = new Tpm2Impl(&factory_);
  }

 protected:
  trunks::TrunksFactoryForTest factory_;
  Tpm* tpm_;
  NiceMock<trunks::MockTpm> mock_tpm_;
  NiceMock<trunks::MockTpmState> mock_tpm_state_;
  NiceMock<trunks::MockTpmUtility> mock_tpm_utility_;
  NiceMock<trunks::MockHmacSession> mock_hmac_session_;
};

TEST_F(Tpm2Test, GetOwnerPassword) {
  chromeos::Blob owner_pass;
  EXPECT_TRUE(tpm_->GetOwnerPassword(&owner_pass));
  EXPECT_EQ(owner_pass.size(), 0);
  SecureBlob password("password");
  tpm_->SetOwnerPassword(password);
  EXPECT_TRUE(tpm_->GetOwnerPassword(&owner_pass));
  EXPECT_EQ(owner_pass.size(), password.size());
  EXPECT_EQ(owner_pass, password);
}

TEST_F(Tpm2Test, EnabledOwnedCheckSuccess) {
  bool enabled;
  bool owned;
  EXPECT_TRUE(tpm_->PerformEnabledOwnedCheck(&enabled, &owned));
  EXPECT_TRUE(enabled);
  EXPECT_TRUE(owned);
}

TEST_F(Tpm2Test, EnabledOwnedCheckStateError) {
  bool enabled;
  bool owned;
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->PerformEnabledOwnedCheck(&enabled, &owned));
  EXPECT_FALSE(enabled);
  EXPECT_FALSE(owned);
}

TEST_F(Tpm2Test, EnabledOwnedNotEnabled) {
  bool enabled;
  bool owned;
  EXPECT_CALL(mock_tpm_state_, IsEnabled())
      .WillOnce(Return(false));
  EXPECT_TRUE(tpm_->PerformEnabledOwnedCheck(&enabled, &owned));
  EXPECT_FALSE(enabled);
  EXPECT_TRUE(owned);
}

TEST_F(Tpm2Test, EnabledOwnedNotOwned) {
  bool enabled;
  bool owned;
  EXPECT_CALL(mock_tpm_state_, IsOwned())
      .WillOnce(Return(false));
  EXPECT_TRUE(tpm_->PerformEnabledOwnedCheck(&enabled, &owned));
  EXPECT_TRUE(enabled);
  EXPECT_FALSE(owned);
}

TEST_F(Tpm2Test, GetRandomDataSuccess) {
  std::string random_data("random_data");
  size_t num_bytes = random_data.size();
  chromeos::Blob data;
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(random_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->GetRandomData(num_bytes, &data));
  EXPECT_EQ(data.size(), num_bytes);
  std::string tpm_data(data.begin(), data.end());
  EXPECT_EQ(tpm_data, random_data);
}

TEST_F(Tpm2Test, GetRandomDataFailure) {
  chromeos::Blob data;
  size_t num_bytes = 5;
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->GetRandomData(num_bytes, &data));
}

TEST_F(Tpm2Test, GetRandomDataBadLength) {
  std::string random_data("random_data");
  chromeos::Blob data;
  size_t num_bytes = random_data.size() + 1;
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(random_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_FALSE(tpm_->GetRandomData(num_bytes, &data));
}

TEST_F(Tpm2Test, DefineNvramSuccess) {
  SecureBlob owner_pass("password");
  tpm_->SetOwnerPassword(owner_pass);
  uint32_t index = 2;
  size_t length = 5;
  std::string recovered_password;
  EXPECT_CALL(mock_hmac_session_, SetEntityAuthorizationValue(_))
      .WillOnce(SaveArg<0>(&recovered_password));
  EXPECT_CALL(mock_tpm_utility_, DefineNVSpace(index, length, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(tpm_->DefineLockOnceNvram(index, length));
  EXPECT_EQ(recovered_password.size(), owner_pass.size());
  EXPECT_EQ(recovered_password, owner_pass.to_string());
}

TEST_F(Tpm2Test, DefineNvramWithoutOwnerPassword) {
  uint32_t index = 2;
  size_t length = 5;
  tpm_->SetOwnerPassword(SecureBlob(""));
  EXPECT_FALSE(tpm_->DefineLockOnceNvram(index, length));
}

TEST_F(Tpm2Test, DefineNvramFailure) {
  uint32_t index = 2;
  size_t length = 5;
  tpm_->SetOwnerPassword(SecureBlob("password"));
  EXPECT_CALL(mock_tpm_utility_, DefineNVSpace(index, length, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->DefineLockOnceNvram(index, length));
}

TEST_F(Tpm2Test, DestroyNvramSuccess) {
  SecureBlob owner_pass("password");
  tpm_->SetOwnerPassword(owner_pass);
  uint32_t index = 2;
  std::string recovered_password;
  EXPECT_CALL(mock_hmac_session_, SetEntityAuthorizationValue(_))
      .WillOnce(SaveArg<0>(&recovered_password));
  EXPECT_CALL(mock_tpm_utility_, DestroyNVSpace(index, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(tpm_->DestroyNvram(index));
  EXPECT_EQ(recovered_password.size(), owner_pass.size());
  EXPECT_EQ(recovered_password, owner_pass.to_string());
}

TEST_F(Tpm2Test, DestroyNvramOwnerPassword) {
  uint32_t index = 2;
  tpm_->SetOwnerPassword(SecureBlob(""));
  EXPECT_FALSE(tpm_->DestroyNvram(index));
}

TEST_F(Tpm2Test, DestroyNvramFailure) {
  uint32_t index = 2;
  tpm_->SetOwnerPassword(SecureBlob("password"));
  EXPECT_CALL(mock_tpm_utility_, DestroyNVSpace(index, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->DestroyNvram(index));
}

TEST_F(Tpm2Test, WriteNvramSuccess) {
  SecureBlob owner_pass("password");
  tpm_->SetOwnerPassword(owner_pass);
  SecureBlob data("nvram_data");
  uint32_t index = 2;
  std::string written_data;
  EXPECT_CALL(mock_tpm_utility_, WriteNVSpace(index, 0, _, _))
      .WillOnce(DoAll(SaveArg<2>(&written_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, LockNVSpace(index, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(tpm_->WriteNvram(index, data));
  EXPECT_EQ(written_data, data.to_string());
}

TEST_F(Tpm2Test, WriteNvramNoOwnerPassword) {
  SecureBlob data("nvram_data");
  uint32_t index = 2;
  EXPECT_FALSE(tpm_->WriteNvram(index, data));
}

TEST_F(Tpm2Test, WriteNvramFailure) {
  SecureBlob owner_pass("password");
  tpm_->SetOwnerPassword(owner_pass);
  SecureBlob data("nvram_data");
  uint32_t index = 2;
  EXPECT_CALL(mock_tpm_utility_, WriteNVSpace(index, 0, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->WriteNvram(index, data));
}

TEST_F(Tpm2Test, WriteNvramLockoutFailure) {
  SecureBlob owner_pass("password");
  tpm_->SetOwnerPassword(owner_pass);
  SecureBlob data("nvram_data");
  uint32_t index = 2;
  std::string written_data;
  EXPECT_CALL(mock_tpm_utility_, WriteNVSpace(index, 0, _, _))
      .WillOnce(DoAll(SaveArg<2>(&written_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, LockNVSpace(index, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->WriteNvram(index, data));
  EXPECT_EQ(written_data, data.to_string());
}

TEST_F(Tpm2Test, ReadNvramSuccess) {
  uint32_t index = 2;
  SecureBlob read_data;
  std::string nvram_data("nvram_data");
  size_t nvram_size = nvram_data.size();
  trunks::TPMS_NV_PUBLIC nvram_public;
  nvram_public.data_size = nvram_size;
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(DoAll(SetArgPointee<1>(nvram_public),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, ReadNVSpace(index, 0, nvram_size, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(nvram_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->ReadNvram(index, &read_data));
  EXPECT_EQ(nvram_data, read_data.to_string());
}

TEST_F(Tpm2Test, ReadNvramFailure) {
  uint32_t index = 2;
  SecureBlob data;
  EXPECT_CALL(mock_tpm_utility_, ReadNVSpace(index, 0, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->ReadNvram(index, &data));
}

TEST_F(Tpm2Test, IsNvramDefinedSuccess) {
  uint32_t index = 2;
  EXPECT_TRUE(tpm_->IsNvramDefined(index));
}

TEST_F(Tpm2Test, IsNvramDefinedError) {
  uint32_t index = 2;
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->IsNvramDefined(index));
}

TEST_F(Tpm2Test, IsNvramDefinedUnknownHandle) {
  uint32_t index = 2;
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(Return(trunks::TPM_RC_HANDLE));
  EXPECT_FALSE(tpm_->IsNvramDefined(index));
}

TEST_F(Tpm2Test, IsNvramLockedSuccess) {
  uint32_t index = 2;
  trunks::TPMS_NV_PUBLIC nvram_public;
  nvram_public.attributes = trunks::TPMA_NV_WRITELOCKED;
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(DoAll(SetArgPointee<1>(nvram_public),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->IsNvramLocked(index));
}

TEST_F(Tpm2Test, IsNvramLockedFailure) {
  uint32_t index = 2;
  trunks::TPMS_NV_PUBLIC nvram_public;
  nvram_public.attributes = (~trunks::TPMA_NV_WRITELOCKED);
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(DoAll(SetArgPointee<1>(nvram_public),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_FALSE(tpm_->IsNvramLocked(index));
}

TEST_F(Tpm2Test, IsNvramLockedError) {
  uint32_t index = 2;
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->IsNvramLocked(index));
}

TEST_F(Tpm2Test, GetNvramSizeSuccess) {
  uint32_t index = 2;
  unsigned int size = 42;
  trunks::TPMS_NV_PUBLIC nvram_public;
  nvram_public.data_size = size;
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(DoAll(SetArgPointee<1>(nvram_public),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(tpm_->GetNvramSize(index), size);
}

TEST_F(Tpm2Test, GetNvramSizeFailure) {
  uint32_t index = 2;
  EXPECT_CALL(mock_tpm_utility_, GetNVSpacePublicArea(index, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(tpm_->GetNvramSize(index), 0);
}

}  // namespace cryptohome
