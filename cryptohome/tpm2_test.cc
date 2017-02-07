// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for Tpm2Impl.

#include "cryptohome/tpm2_impl.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tpm_manager/common/mock_tpm_nvram_interface.h>
#include <tpm_manager/common/mock_tpm_ownership_interface.h>
#include <tpm_manager/common/tpm_manager_constants.h>
#include <trunks/mock_authorization_delegate.h>
#include <trunks/mock_blob_parser.h>
#include <trunks/mock_hmac_session.h>
#include <trunks/mock_policy_session.h>
#include <trunks/mock_tpm.h>
#include <trunks/mock_tpm_state.h>
#include <trunks/mock_tpm_utility.h>
#include <trunks/tpm_constants.h>
#include <trunks/tpm_generated.h>
#include <trunks/trunks_factory.h>
#include <trunks/trunks_factory_for_test.h>

#include "cryptohome/cryptolib.h"

using brillo::SecureBlob;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::WithArg;
using tpm_manager::NVRAM_RESULT_IPC_ERROR;
using trunks::TPM_RC_FAILURE;
using trunks::TPM_RC_SUCCESS;
using trunks::TrunksFactory;

namespace {

const char kDefaultPassword[] = "password";
const int kDefaultCounter = 3;

}  // namespace

namespace cryptohome {

class Tpm2Test : public testing::Test {
 public:
  Tpm2Test() {}
  ~Tpm2Test() override {}

  void SetUp() override {
    factory_.set_blob_parser(&mock_blob_parser_);
    factory_.set_tpm(&mock_tpm_);
    factory_.set_tpm_state(&mock_tpm_state_);
    factory_.set_tpm_utility(&mock_tpm_utility_);
    factory_.set_hmac_session(&mock_hmac_session_);
    factory_.set_policy_session(&mock_policy_session_);
    factory_.set_trial_session(&mock_trial_session_);
    tpm_ = new Tpm2Impl(&factory_, &mock_tpm_owner_, &mock_tpm_nvram_);
    // Setup default status data.
    tpm_status_.set_status(tpm_manager::STATUS_SUCCESS);
    tpm_status_.set_enabled(true);
    tpm_status_.set_owned(true);
    tpm_status_.mutable_local_data()->set_owner_password(kDefaultPassword);
    tpm_status_.set_dictionary_attack_counter(kDefaultCounter);
    ON_CALL(mock_tpm_owner_, GetTpmStatus(_, _))
        .WillByDefault(WithArg<1>(Invoke(this, &Tpm2Test::FakeGetTpmStatus)));
    ON_CALL(mock_tpm_owner_, RemoveOwnerDependency(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeRemoveOwnerDependency));
    SetupFakeNvram();
  }

  void SetupFakeNvram() {
    ON_CALL(mock_tpm_nvram_, DefineSpace(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeDefineSpace));
    ON_CALL(mock_tpm_nvram_, DestroySpace(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeDestroySpace));
    ON_CALL(mock_tpm_nvram_, WriteSpace(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeWriteSpace));
    ON_CALL(mock_tpm_nvram_, ReadSpace(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeReadSpace));
    ON_CALL(mock_tpm_nvram_, LockSpace(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeLockSpace));
    ON_CALL(mock_tpm_nvram_, ListSpaces(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeListSpaces));
    ON_CALL(mock_tpm_nvram_, GetSpaceInfo(_, _))
        .WillByDefault(Invoke(this, &Tpm2Test::FakeGetSpaceInfo));
  }

 protected:
  void FakeGetTpmStatus(
      const tpm_manager::TpmOwnershipInterface::GetTpmStatusCallback&
          callback) {
    callback.Run(tpm_status_);
  }

  void FakeRemoveOwnerDependency(
      const tpm_manager::RemoveOwnerDependencyRequest& request,
      const tpm_manager::TpmOwnershipInterface::RemoveOwnerDependencyCallback&
          callback) {
    last_remove_owner_dependency_request = request;
    callback.Run(next_remove_owner_dependency_reply);
  }

  void FakeDefineSpace(
      const tpm_manager::DefineSpaceRequest& request,
      const tpm_manager::TpmNvramInterface::DefineSpaceCallback& callback) {
    last_define_space_request = request;
    callback.Run(next_define_space_reply);
  }

  void FakeDestroySpace(
      const tpm_manager::DestroySpaceRequest& request,
      const tpm_manager::TpmNvramInterface::DestroySpaceCallback& callback) {
    last_destroy_space_request = request;
    callback.Run(next_destroy_space_reply);
  }

  void FakeWriteSpace(
      const tpm_manager::WriteSpaceRequest& request,
      const tpm_manager::TpmNvramInterface::WriteSpaceCallback& callback) {
    last_write_space_request = request;
    callback.Run(next_write_space_reply);
  }

  void FakeReadSpace(
      const tpm_manager::ReadSpaceRequest& request,
      const tpm_manager::TpmNvramInterface::ReadSpaceCallback& callback) {
    last_read_space_request = request;
    callback.Run(next_read_space_reply);
  }

  void FakeLockSpace(
      const tpm_manager::LockSpaceRequest& request,
      const tpm_manager::TpmNvramInterface::LockSpaceCallback& callback) {
    last_lock_space_request = request;
    callback.Run(next_lock_space_reply);
  }

  void FakeListSpaces(
      const tpm_manager::ListSpacesRequest& request,
      const tpm_manager::TpmNvramInterface::ListSpacesCallback& callback) {
    last_list_spaces_request = request;
    callback.Run(next_list_spaces_reply);
  }

  void FakeGetSpaceInfo(
      const tpm_manager::GetSpaceInfoRequest& request,
      const tpm_manager::TpmNvramInterface::GetSpaceInfoCallback& callback) {
    last_get_space_info_request = request;
    callback.Run(next_get_space_info_reply);
  }

  tpm_manager::GetTpmStatusReply tpm_status_;
  tpm_manager::DefineSpaceRequest last_define_space_request;
  tpm_manager::DestroySpaceRequest last_destroy_space_request;
  tpm_manager::WriteSpaceRequest last_write_space_request;
  tpm_manager::ReadSpaceRequest last_read_space_request;
  tpm_manager::LockSpaceRequest last_lock_space_request;
  tpm_manager::ListSpacesRequest last_list_spaces_request;
  tpm_manager::GetSpaceInfoRequest last_get_space_info_request;
  tpm_manager::RemoveOwnerDependencyRequest
      last_remove_owner_dependency_request;
  tpm_manager::DefineSpaceReply next_define_space_reply;
  tpm_manager::DestroySpaceReply next_destroy_space_reply;
  tpm_manager::WriteSpaceReply next_write_space_reply;
  tpm_manager::ReadSpaceReply next_read_space_reply;
  tpm_manager::LockSpaceReply next_lock_space_reply;
  tpm_manager::ListSpacesReply next_list_spaces_reply;
  tpm_manager::GetSpaceInfoReply next_get_space_info_reply;
  tpm_manager::RemoveOwnerDependencyReply next_remove_owner_dependency_reply;

  trunks::TrunksFactoryForTest factory_;
  Tpm* tpm_;
  NiceMock<trunks::MockAuthorizationDelegate> mock_authorization_delegate_;
  NiceMock<trunks::MockBlobParser> mock_blob_parser_;
  NiceMock<trunks::MockTpm> mock_tpm_;
  NiceMock<trunks::MockTpmState> mock_tpm_state_;
  NiceMock<trunks::MockTpmUtility> mock_tpm_utility_;
  NiceMock<trunks::MockHmacSession> mock_hmac_session_;
  NiceMock<trunks::MockPolicySession> mock_policy_session_;
  NiceMock<trunks::MockPolicySession> mock_trial_session_;
  NiceMock<tpm_manager::MockTpmOwnershipInterface> mock_tpm_owner_;
  NiceMock<tpm_manager::MockTpmNvramInterface> mock_tpm_nvram_;
};

TEST_F(Tpm2Test, GetOwnerPassword) {
  brillo::SecureBlob owner_password;
  EXPECT_TRUE(tpm_->GetOwnerPassword(&owner_password));
  EXPECT_EQ(kDefaultPassword, owner_password.to_string());
}

TEST_F(Tpm2Test, EnabledOwnedCheckSuccess) {
  bool enabled = false;
  bool owned = false;
  EXPECT_TRUE(tpm_->PerformEnabledOwnedCheck(&enabled, &owned));
  EXPECT_TRUE(enabled);
  EXPECT_TRUE(owned);
}

TEST_F(Tpm2Test, EnabledOwnedCheckStateError) {
  tpm_status_.set_status(tpm_manager::STATUS_NOT_AVAILABLE);
  bool enabled = false;
  bool owned = false;
  EXPECT_FALSE(tpm_->PerformEnabledOwnedCheck(&enabled, &owned));
  EXPECT_FALSE(enabled);
  EXPECT_FALSE(owned);
}

TEST_F(Tpm2Test, GetRandomDataSuccess) {
  std::string random_data("random_data");
  size_t num_bytes = random_data.size();
  brillo::Blob data;
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(random_data), Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->GetRandomData(num_bytes, &data));
  EXPECT_EQ(data.size(), num_bytes);
  std::string tpm_data(data.begin(), data.end());
  EXPECT_EQ(tpm_data, random_data);
}

TEST_F(Tpm2Test, GetRandomDataFailure) {
  brillo::Blob data;
  size_t num_bytes = 5;
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->GetRandomData(num_bytes, &data));
}

TEST_F(Tpm2Test, GetRandomDataBadLength) {
  std::string random_data("random_data");
  brillo::Blob data;
  size_t num_bytes = random_data.size() + 1;
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(random_data), Return(TPM_RC_SUCCESS)));
  EXPECT_FALSE(tpm_->GetRandomData(num_bytes, &data));
}

TEST_F(Tpm2Test, DefineNvramSuccess) {
  uint32_t index = 2;
  size_t length = 5;
  EXPECT_TRUE(tpm_->DefineNvram(
      index, length, Tpm::kTpmNvramWriteDefine));
  EXPECT_EQ(index, last_define_space_request.index());
  EXPECT_EQ(length, last_define_space_request.size());
  ASSERT_EQ(1, last_define_space_request.attributes_size());
  EXPECT_EQ(tpm_manager::NVRAM_PERSISTENT_WRITE_LOCK,
            last_define_space_request.attributes(0));
  EXPECT_EQ(tpm_manager::NVRAM_POLICY_NONE, last_define_space_request.policy());
}

TEST_F(Tpm2Test, DefineNvramSuccessWithPolicy) {
  uint32_t index = 2;
  size_t length = 5;
  EXPECT_TRUE(tpm_->DefineNvram(
      index, length, Tpm::kTpmNvramWriteDefine | Tpm::kTpmNvramBindToPCR0));
  EXPECT_EQ(index, last_define_space_request.index());
  EXPECT_EQ(length, last_define_space_request.size());
  ASSERT_EQ(1, last_define_space_request.attributes_size());
  EXPECT_EQ(tpm_manager::NVRAM_PERSISTENT_WRITE_LOCK,
            last_define_space_request.attributes(0));
  EXPECT_EQ(tpm_manager::NVRAM_POLICY_PCR0, last_define_space_request.policy());
}

TEST_F(Tpm2Test, DefineNvramSuccessFirmwareReadable) {
  uint32_t index = 2;
  size_t length = 5;
  EXPECT_TRUE(tpm_->DefineNvram(
      index, length,
      Tpm::kTpmNvramWriteDefine | Tpm::kTpmNvramFirmwareReadable));
  EXPECT_EQ(index, last_define_space_request.index());
  EXPECT_EQ(length, last_define_space_request.size());
  ASSERT_EQ(2, last_define_space_request.attributes_size());
  EXPECT_EQ(tpm_manager::NVRAM_PERSISTENT_WRITE_LOCK,
            last_define_space_request.attributes(0));
  EXPECT_EQ(tpm_manager::NVRAM_PLATFORM_READ,
            last_define_space_request.attributes(1));
  EXPECT_EQ(tpm_manager::NVRAM_POLICY_NONE, last_define_space_request.policy());
}

TEST_F(Tpm2Test, DefineNvramFailure) {
  next_define_space_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  EXPECT_FALSE(tpm_->DefineNvram(0, 0, 0));
}

TEST_F(Tpm2Test, DestroyNvramSuccess) {
  uint32_t index = 2;
  EXPECT_TRUE(tpm_->DestroyNvram(index));
  EXPECT_EQ(index, last_destroy_space_request.index());
}

TEST_F(Tpm2Test, DestroyNvramFailure) {
  next_destroy_space_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  EXPECT_FALSE(tpm_->DestroyNvram(0));
}

TEST_F(Tpm2Test, WriteNvramSuccess) {
  uint32_t index = 2;
  std::string data("nvram_data");
  EXPECT_TRUE(tpm_->WriteNvram(index, SecureBlob(data)));
  EXPECT_EQ(index, last_write_space_request.index());
  EXPECT_EQ(data, last_write_space_request.data());
}

TEST_F(Tpm2Test, WriteNvramFailure) {
  next_write_space_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  EXPECT_FALSE(tpm_->WriteNvram(0, SecureBlob()));
}

TEST_F(Tpm2Test, WriteLockNvramSuccess) {
  uint32_t index = 2;
  EXPECT_TRUE(tpm_->WriteLockNvram(index));
  EXPECT_EQ(index, last_lock_space_request.index());
  EXPECT_TRUE(last_lock_space_request.lock_write());
  EXPECT_FALSE(last_lock_space_request.lock_read());
}

TEST_F(Tpm2Test, WriteLockNvramFailure) {
  next_lock_space_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  EXPECT_FALSE(tpm_->WriteLockNvram(0));
}

TEST_F(Tpm2Test, ReadNvramSuccess) {
  uint32_t index = 2;
  SecureBlob read_data;
  std::string nvram_data("nvram_data");
  next_read_space_reply.set_data(nvram_data);
  EXPECT_TRUE(tpm_->ReadNvram(index, &read_data));
  EXPECT_EQ(nvram_data, read_data.to_string());
  EXPECT_EQ(index, last_read_space_request.index());
}

TEST_F(Tpm2Test, ReadNvramFailure) {
  next_read_space_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  SecureBlob read_data;
  EXPECT_FALSE(tpm_->ReadNvram(0, &read_data));
}

TEST_F(Tpm2Test, IsNvramDefinedSuccess) {
  uint32_t index = 2;
  next_list_spaces_reply.add_index_list(index);
  EXPECT_TRUE(tpm_->IsNvramDefined(index));
}

TEST_F(Tpm2Test, IsNvramDefinedFailure) {
  uint32_t index = 2;
  next_list_spaces_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  next_list_spaces_reply.add_index_list(index);
  EXPECT_FALSE(tpm_->IsNvramDefined(index));
}

TEST_F(Tpm2Test, IsNvramDefinedUnknownHandle) {
  uint32_t index = 2;
  next_list_spaces_reply.add_index_list(index + 1);
  EXPECT_FALSE(tpm_->IsNvramDefined(index));
}

TEST_F(Tpm2Test, IsNvramLockedSuccess) {
  uint32_t index = 2;
  next_get_space_info_reply.set_is_write_locked(true);
  EXPECT_TRUE(tpm_->IsNvramLocked(index));
  EXPECT_EQ(index, last_get_space_info_request.index());
}

TEST_F(Tpm2Test, IsNvramLockedNotLocked) {
  next_get_space_info_reply.set_is_write_locked(false);
  EXPECT_FALSE(tpm_->IsNvramLocked(0));
}

TEST_F(Tpm2Test, IsNvramLockedFailure) {
  next_get_space_info_reply.set_is_write_locked(true);
  next_get_space_info_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  EXPECT_FALSE(tpm_->IsNvramLocked(0));
}

TEST_F(Tpm2Test, GetNvramSizeSuccess) {
  uint32_t index = 2;
  unsigned int size = 42;
  next_get_space_info_reply.set_size(size);
  EXPECT_EQ(tpm_->GetNvramSize(index), size);
}

TEST_F(Tpm2Test, GetNvramSizeFailure) {
  uint32_t index = 2;
  unsigned int size = 42;
  next_get_space_info_reply.set_size(size);
  next_get_space_info_reply.set_result(NVRAM_RESULT_IPC_ERROR);
  EXPECT_EQ(tpm_->GetNvramSize(index), 0);
}

TEST_F(Tpm2Test, SealToPCR0Success) {
  SecureBlob value("value");
  SecureBlob sealed_value;
  std::string policy_digest("digest");
  EXPECT_CALL(mock_tpm_utility_, GetPolicyDigestForPcrValue(0, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(policy_digest), Return(TPM_RC_SUCCESS)));
  std::string data_to_seal;
  EXPECT_CALL(mock_tpm_utility_, SealData(_, policy_digest, _, _))
      .WillOnce(DoAll(SaveArg<0>(&data_to_seal), Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->SealToPCR0(value, &sealed_value));
  EXPECT_EQ(data_to_seal, value.to_string());
}

TEST_F(Tpm2Test, SealToPCR0PolicyFailure) {
  SecureBlob value("value");
  SecureBlob sealed_value;
  EXPECT_CALL(mock_tpm_utility_, GetPolicyDigestForPcrValue(0, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->SealToPCR0(value, &sealed_value));
}

TEST_F(Tpm2Test, SealToPCR0Failure) {
  SecureBlob value("value");
  SecureBlob sealed_value;
  EXPECT_CALL(mock_tpm_utility_, SealData(_, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->SealToPCR0(value, &sealed_value));
}

TEST_F(Tpm2Test, UnsealSuccess) {
  SecureBlob sealed_value("sealed");
  SecureBlob value;
  std::string unsealed_data("unsealed");
  EXPECT_CALL(mock_tpm_utility_, UnsealData(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(unsealed_data), Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->Unseal(sealed_value, &value));
  EXPECT_EQ(unsealed_data, value.to_string());
}

TEST_F(Tpm2Test, UnsealStartPolicySessionFail) {
  SecureBlob sealed_value("sealed");
  SecureBlob value;
  EXPECT_CALL(mock_policy_session_, StartUnboundSession(false))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->Unseal(sealed_value, &value));
}

TEST_F(Tpm2Test, UnsealPolicyPCRFailure) {
  SecureBlob sealed_value("sealed");
  SecureBlob value;
  EXPECT_CALL(mock_policy_session_, PolicyPCR(0, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->Unseal(sealed_value, &value));
}

TEST_F(Tpm2Test, UnsealFailure) {
  SecureBlob sealed_value("sealed");
  SecureBlob value;
  EXPECT_CALL(mock_tpm_utility_, UnsealData(_, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->Unseal(sealed_value, &value));
}

TEST_F(Tpm2Test, SignPolicySuccess) {
  int pcr_index = 5;
  EXPECT_CALL(mock_policy_session_, PolicyPCR(pcr_index, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_policy_session_, GetDelegate())
      .WillOnce(Return(&mock_authorization_delegate_));
  std::string tpm_signature(32, 'b');
  EXPECT_CALL(mock_tpm_utility_,
              Sign(_, _, _, _, _, &mock_authorization_delegate_, _))
      .WillOnce(DoAll(SetArgPointee<6>(tpm_signature), Return(TPM_RC_SUCCESS)));
  SecureBlob signature;
  EXPECT_TRUE(tpm_->Sign(SecureBlob("key_blob"), SecureBlob("input"), pcr_index,
                         &signature));
  EXPECT_EQ(signature.to_string(), tpm_signature);
}

TEST_F(Tpm2Test, SignHmacSuccess) {
  EXPECT_CALL(mock_hmac_session_, GetDelegate())
      .WillOnce(Return(&mock_authorization_delegate_));
  std::string tpm_signature(32, 'b');
  EXPECT_CALL(mock_tpm_utility_,
              Sign(_, _, _, _, _, &mock_authorization_delegate_, _))
      .WillOnce(DoAll(SetArgPointee<6>(tpm_signature), Return(TPM_RC_SUCCESS)));

  SecureBlob signature;
  EXPECT_TRUE(
      tpm_->Sign(SecureBlob("key_blob"), SecureBlob("input"), -1, &signature));
  EXPECT_EQ(signature.to_string(), tpm_signature);
}

TEST_F(Tpm2Test, SignLoadFailure) {
  EXPECT_CALL(mock_tpm_utility_, LoadKey(_, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));

  SecureBlob signature;
  EXPECT_FALSE(
      tpm_->Sign(SecureBlob("key_blob"), SecureBlob("input"), -1, &signature));
}

TEST_F(Tpm2Test, SignFailure) {
  uint32_t handle = 42;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(handle), Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, Sign(handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));

  SecureBlob signature;
  EXPECT_FALSE(
      tpm_->Sign(SecureBlob("key_blob"), SecureBlob("input"), -1, &signature));
}

TEST_F(Tpm2Test, CreatePCRBoundKeySuccess) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;
  uint32_t modulus = 2048;
  uint32_t exponent = 0x10001;
  EXPECT_CALL(mock_tpm_utility_,
              CreateRSAKeyPair(_, modulus, exponent, _, _, true, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(tpm_->CreatePCRBoundKey(index, pcr_value, &key_blob, nullptr,
                                      &creation_blob));
}

TEST_F(Tpm2Test, CreatePCRBoundKeyPolicyFailure) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;
  EXPECT_CALL(mock_tpm_utility_, GetPolicyDigestForPcrValue(index, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->CreatePCRBoundKey(index, pcr_value, &key_blob, nullptr,
                                       &creation_blob));
}

TEST_F(Tpm2Test, CreatePCRBoundKeyFailure) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;
  EXPECT_CALL(mock_tpm_utility_, CreateRSAKeyPair(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->CreatePCRBoundKey(index, pcr_value, &key_blob, nullptr,
                                       &creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeySuccess) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(pcr_value).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));
  std::string pcr_policy_value;
  EXPECT_CALL(mock_trial_session_, PolicyPCR(index, _))
      .WillOnce(DoAll(SaveArg<1>(&pcr_policy_value), Return(TPM_RC_SUCCESS)));
  std::string policy_digest(32, 'a');
  EXPECT_CALL(mock_trial_session_, GetDigest(_))
      .WillOnce(DoAll(SetArgPointee<0>(policy_digest), Return(TPM_RC_SUCCESS)));
  trunks::TPMT_PUBLIC public_area;
  public_area.auth_policy.size = policy_digest.size();
  memcpy(public_area.auth_policy.buffer, policy_digest.data(),
         policy_digest.size());
  public_area.object_attributes &= (~trunks::kUserWithAuth);
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_area), Return(TPM_RC_SUCCESS)));
  ASSERT_TRUE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
  EXPECT_EQ(pcr_policy_value, pcr_value.to_string());
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadCreationBlob) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadCreationDataCount) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  creation_data.creation_data.pcr_select.count = 0;
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadCreationPCRBank) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA1;
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadCreationPCR) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 0xFF;
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadCreationPCRDigest) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(SecureBlob("")).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyImportedKey) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(pcr_value).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));

  EXPECT_CALL(mock_tpm_utility_, CertifyCreation(_, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadSession) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(pcr_value).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));

  EXPECT_CALL(mock_trial_session_, StartUnboundSession(true))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadPolicy) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(pcr_value).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));

  EXPECT_CALL(mock_trial_session_, PolicyPCR(index, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadDigest) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(pcr_value).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));

  EXPECT_CALL(mock_trial_session_, GetDigest(_))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadPolicyDigest) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(pcr_value).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));

  std::string policy_digest(32, 'a');
  EXPECT_CALL(mock_trial_session_, GetDigest(_))
      .WillOnce(DoAll(SetArgPointee<0>(policy_digest), Return(TPM_RC_SUCCESS)));

  trunks::TPMT_PUBLIC public_area;
  public_area.auth_policy.size = 2;
  public_area.object_attributes &= (~trunks::kUserWithAuth);
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_area), Return(TPM_RC_SUCCESS)));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, VerifyPCRBoundKeyBadAttributes) {
  int index = 2;
  SecureBlob pcr_value("pcr_value");
  SecureBlob key_blob;
  SecureBlob creation_blob;

  trunks::TPM2B_CREATION_DATA creation_data;
  trunks::TPML_PCR_SELECTION& pcr_select =
      creation_data.creation_data.pcr_select;
  pcr_select.count = 1;
  pcr_select.pcr_selections[0].hash = trunks::TPM_ALG_SHA256;
  pcr_select.pcr_selections[0].pcr_select[index / 8] = 1 << (index % 8);
  creation_data.creation_data.pcr_digest =
      trunks::Make_TPM2B_DIGEST(CryptoLib::Sha256(pcr_value).to_string());
  EXPECT_CALL(mock_blob_parser_, ParseCreationBlob(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(creation_data), Return(true)));

  std::string policy_digest(32, 'a');
  EXPECT_CALL(mock_trial_session_, GetDigest(_))
      .WillOnce(DoAll(SetArgPointee<0>(policy_digest), Return(TPM_RC_SUCCESS)));

  trunks::TPMT_PUBLIC public_area;
  public_area.auth_policy.size = policy_digest.size();
  memcpy(public_area.auth_policy.buffer, policy_digest.data(),
         policy_digest.size());
  public_area.object_attributes = trunks::kUserWithAuth;
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_area), Return(TPM_RC_SUCCESS)));
  EXPECT_FALSE(
      tpm_->VerifyPCRBoundKey(index, pcr_value, key_blob, creation_blob));
}

TEST_F(Tpm2Test, ExtendPCRSuccess) {
  int index = 5;
  SecureBlob extension("extension");
  std::string pcr_value;
  EXPECT_CALL(mock_tpm_utility_, ExtendPCR(index, _, _))
      .WillOnce(DoAll(SaveArg<1>(&pcr_value), Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->ExtendPCR(index, extension));
  EXPECT_EQ(pcr_value, extension.to_string());
}

TEST_F(Tpm2Test, ExtendPCRFailure) {
  int index = 5;
  SecureBlob extension("extension");
  EXPECT_CALL(mock_tpm_utility_, ExtendPCR(index, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->ExtendPCR(index, extension));
}

TEST_F(Tpm2Test, ReadPCRSuccess) {
  int index = 5;
  SecureBlob pcr_value;
  std::string pcr_digest("digest");
  EXPECT_CALL(mock_tpm_utility_, ReadPCR(index, _))
      .WillOnce(DoAll(SetArgPointee<1>(pcr_digest), Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(tpm_->ReadPCR(index, &pcr_value));
  EXPECT_EQ(pcr_digest, pcr_value.to_string());
}

TEST_F(Tpm2Test, ReadPCRFailure) {
  int index = 5;
  SecureBlob pcr_value;
  EXPECT_CALL(mock_tpm_utility_, ReadPCR(index, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->ReadPCR(index, &pcr_value));
}

TEST_F(Tpm2Test, WrapRsaKeySuccess) {
  std::string key_blob("key_blob");
  SecureBlob modulus;
  SecureBlob prime_factor;
  EXPECT_CALL(mock_tpm_utility_, ImportRSAKey(_, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<6>(key_blob), Return(TPM_RC_SUCCESS)));
  SecureBlob wrapped_key;
  EXPECT_TRUE(tpm_->WrapRsaKey(modulus, prime_factor, &wrapped_key));
  EXPECT_EQ(key_blob, wrapped_key.to_string());
}

TEST_F(Tpm2Test, WrapRsaKeyFailure) {
  SecureBlob wrapped_key;
  EXPECT_CALL(mock_tpm_utility_, ImportRSAKey(_, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(tpm_->WrapRsaKey(SecureBlob(), SecureBlob(), &wrapped_key));
}

TEST_F(Tpm2Test, LoadWrappedKeySuccess) {
  SecureBlob wrapped_key("wrapped_key");
  trunks::TPM_HANDLE handle = trunks::TPM_RH_FIRST;
  std::string loaded_key;
  ScopedKeyHandle key_handle;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&loaded_key), SetArgPointee<2>(handle),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(tpm_->LoadWrappedKey(wrapped_key, &key_handle), Tpm::kTpmRetryNone);
  EXPECT_EQ(handle, key_handle.value());
  EXPECT_EQ(loaded_key, wrapped_key.to_string());
}

TEST_F(Tpm2Test, LoadWrappedKeyFailure) {
  SecureBlob wrapped_key("wrapped_key");
  ScopedKeyHandle key_handle;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(_, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(tpm_->LoadWrappedKey(wrapped_key, &key_handle),
            Tpm::kTpmRetryFailNoRetry);
}

TEST_F(Tpm2Test, CloseHandle) {
  TpmKeyHandle key_handle = 42;
  EXPECT_CALL(mock_tpm_, FlushContextSync(key_handle, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  tpm_->CloseHandle(key_handle);
}

TEST_F(Tpm2Test, EncryptBlobSuccess) {
  TpmKeyHandle handle = 42;
  std::string tpm_ciphertext(32, 'a');
  SecureBlob key(32, 'b');
  SecureBlob plaintext("plaintext");
  EXPECT_CALL(mock_tpm_utility_, AsymmetricEncrypt(handle, _, _, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<5>(tpm_ciphertext), Return(TPM_RC_SUCCESS)));
  SecureBlob ciphertext;
  EXPECT_EQ(Tpm::kTpmRetryNone,
            tpm_->EncryptBlob(handle, plaintext, key, &ciphertext));
}

TEST_F(Tpm2Test, EncryptBlobBadAesKey) {
  TpmKeyHandle handle = 42;
  std::string tpm_ciphertext(32, 'a');
  SecureBlob key(16, 'b');
  SecureBlob plaintext("plaintext");
  EXPECT_CALL(mock_tpm_utility_, AsymmetricEncrypt(handle, _, _, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<5>(tpm_ciphertext), Return(TPM_RC_SUCCESS)));
  SecureBlob ciphertext;
  EXPECT_EQ(Tpm::kTpmRetryFailNoRetry,
            tpm_->EncryptBlob(handle, plaintext, key, &ciphertext));
}

TEST_F(Tpm2Test, EncryptBlobBadTpmEncrypt) {
  TpmKeyHandle handle = 42;
  std::string tpm_ciphertext(16, 'a');
  SecureBlob key(32, 'b');
  SecureBlob plaintext("plaintext");
  EXPECT_CALL(mock_tpm_utility_, AsymmetricEncrypt(handle, _, _, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<5>(tpm_ciphertext), Return(TPM_RC_SUCCESS)));
  SecureBlob ciphertext;
  EXPECT_EQ(Tpm::kTpmRetryFailNoRetry,
            tpm_->EncryptBlob(handle, plaintext, key, &ciphertext));
}

TEST_F(Tpm2Test, EncryptBlobFailure) {
  TpmKeyHandle handle = 42;
  SecureBlob key(32, 'b');
  SecureBlob plaintext("plaintext");
  EXPECT_CALL(mock_tpm_utility_, AsymmetricEncrypt(handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  SecureBlob ciphertext;
  EXPECT_EQ(Tpm::kTpmRetryFailNoRetry,
            tpm_->EncryptBlob(handle, plaintext, key, &ciphertext));
}

TEST_F(Tpm2Test, DecryptBlobSuccess) {
  TpmKeyHandle handle = 42;
  SecureBlob key(32, 'a');
  SecureBlob ciphertext(32, 'b');
  std::string tpm_plaintext("plaintext");
  EXPECT_CALL(mock_tpm_utility_, AsymmetricDecrypt(handle, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(tpm_plaintext), Return(TPM_RC_SUCCESS)));
  SecureBlob plaintext;
  EXPECT_EQ(Tpm::kTpmRetryNone,
            tpm_->DecryptBlob(handle, ciphertext, key, &plaintext));
}

TEST_F(Tpm2Test, DecryptBlobBadAesKey) {
  TpmKeyHandle handle = 42;
  SecureBlob key(16, 'a');
  SecureBlob ciphertext(32, 'b');
  SecureBlob plaintext;
  EXPECT_EQ(Tpm::kTpmRetryFailNoRetry,
            tpm_->DecryptBlob(handle, ciphertext, key, &plaintext));
}

TEST_F(Tpm2Test, DecryptBlobBadCiphertext) {
  TpmKeyHandle handle = 42;
  SecureBlob key(32, 'a');
  SecureBlob ciphertext(16, 'b');
  SecureBlob plaintext;
  EXPECT_EQ(Tpm::kTpmRetryFailNoRetry,
            tpm_->DecryptBlob(handle, ciphertext, key, &plaintext));
}

TEST_F(Tpm2Test, DecryptBlobFailure) {
  TpmKeyHandle handle = 42;
  SecureBlob key(32, 'a');
  SecureBlob ciphertext(32, 'b');
  EXPECT_CALL(mock_tpm_utility_, AsymmetricDecrypt(handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  SecureBlob plaintext;
  EXPECT_EQ(Tpm::kTpmRetryFailNoRetry,
            tpm_->DecryptBlob(handle, ciphertext, key, &plaintext));
}

TEST_F(Tpm2Test, GetPublicKeyHashSuccess) {
  TpmKeyHandle handle = 42;
  trunks::TPMT_PUBLIC public_data;
  SecureBlob public_key("hello");
  public_data.unique.rsa =
      trunks::Make_TPM2B_PUBLIC_KEY_RSA(public_key.to_string());
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data), Return(TPM_RC_SUCCESS)));
  SecureBlob public_key_hash;
  EXPECT_EQ(Tpm::kTpmRetryNone,
            tpm_->GetPublicKeyHash(handle, &public_key_hash));
  SecureBlob expected_key_hash = CryptoLib::Sha256(public_key);
  EXPECT_EQ(expected_key_hash, public_key_hash);
}

TEST_F(Tpm2Test, GetPublicKeyHashFailure) {
  TpmKeyHandle handle = 42;
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(handle, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  SecureBlob public_key_hash;
  EXPECT_EQ(Tpm::kTpmRetryFailNoRetry,
            tpm_->GetPublicKeyHash(handle, &public_key_hash));
}

TEST_F(Tpm2Test, DeclareTpmFirmwareStable) {
  EXPECT_CALL(mock_tpm_utility_, DeclareTpmFirmwareStable())
      .Times(2)
      .WillOnce(Return(TPM_RC_FAILURE))
      .WillOnce(Return(TPM_RC_SUCCESS));
  // First attempt shall call TpmUtility since we haven't called it yet.
  tpm_->DeclareTpmFirmwareStable();
  // Second attempt shall call TpmUtility since the first attempt failed.
  tpm_->DeclareTpmFirmwareStable();
  // Subsequent attempts shall do nothing since we already succeeded on the
  // second attempt.
  tpm_->DeclareTpmFirmwareStable();
  tpm_->DeclareTpmFirmwareStable();
}

TEST_F(Tpm2Test, RemoveOwnerDependencySuccess) {
  EXPECT_TRUE(tpm_->RemoveOwnerDependency(
      Tpm::TpmOwnerDependency::kInstallAttributes));
  EXPECT_EQ(tpm_manager::kTpmOwnerDependency_Nvram,
            last_remove_owner_dependency_request.owner_dependency());
  EXPECT_TRUE(tpm_->RemoveOwnerDependency(
      Tpm::TpmOwnerDependency::kAttestation));
  EXPECT_EQ(tpm_manager::kTpmOwnerDependency_Attestation,
            last_remove_owner_dependency_request.owner_dependency());
}

TEST_F(Tpm2Test, RemoveOwnerDependencyFailure) {
  next_remove_owner_dependency_reply.set_status(
      tpm_manager::STATUS_DEVICE_ERROR);
  EXPECT_FALSE(tpm_->RemoveOwnerDependency(
      Tpm::TpmOwnerDependency::kInstallAttributes));
  EXPECT_EQ(tpm_manager::kTpmOwnerDependency_Nvram,
            last_remove_owner_dependency_request.owner_dependency());
}

TEST_F(Tpm2Test, RemoveOwnerDependencyUnknown) {
  Tpm::TpmOwnerDependency unknown_dep =
      static_cast<Tpm::TpmOwnerDependency>(100);
  EXPECT_CALL(mock_tpm_owner_, RemoveOwnerDependency(_, _))
        .Times(0);
  EXPECT_TRUE(tpm_->RemoveOwnerDependency(unknown_dep));
}

}  // namespace cryptohome
