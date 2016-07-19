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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "attestation/common/tpm_utility_v2.h"
#include "tpm_manager/common/mock_tpm_nvram_interface.h"
#include "tpm_manager/common/mock_tpm_ownership_interface.h"
#include "trunks/mock_tpm.h"
#include "trunks/mock_tpm_utility.h"
#include "trunks/trunks_factory_for_test.h"

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArg;
using ::trunks::TPM_RC_FAILURE;
using ::trunks::TPM_RC_SUCCESS;

const char kDefaultPassword[] = "password";

}  // namespace

namespace attestation {

class TpmUtilityTest : public testing::Test {
 public:
  ~TpmUtilityTest() override = default;
  void SetUp() override {
    // Setup default status data.
    tpm_status_.set_status(tpm_manager::STATUS_SUCCESS);
    tpm_status_.set_enabled(true);
    tpm_status_.set_owned(true);
    tpm_status_.mutable_local_data()->set_endorsement_password(
        kDefaultPassword);
    ON_CALL(mock_tpm_owner_, GetTpmStatus(_, _))
        .WillByDefault(
            WithArg<1>(Invoke(this, &TpmUtilityTest::FakeGetTpmStatus)));
    // Setup fake nvram.
    ON_CALL(mock_tpm_nvram_, ReadSpace(_, _))
        .WillByDefault(Invoke(this, &TpmUtilityTest::FakeReadSpace));
    // Setup trunks factory with mocks.
    trunks_factory_for_test_.set_tpm(&mock_tpm_);
    trunks_factory_for_test_.set_tpm_utility(&mock_tpm_utility_);
    tpm_utility_.reset(new TpmUtilityV2(&mock_tpm_owner_, &mock_tpm_nvram_,
                                        &trunks_factory_for_test_));
    tpm_utility_->Initialize();
  }

 protected:
  void FakeGetTpmStatus(
      const tpm_manager::TpmOwnershipInterface::GetTpmStatusCallback&
          callback) {
    callback.Run(tpm_status_);
  }

  void FakeReadSpace(
      const tpm_manager::ReadSpaceRequest& request,
      const tpm_manager::TpmNvramInterface::ReadSpaceCallback& callback) {
    last_read_space_request_ = request;
    callback.Run(next_read_space_reply_);
  }

  tpm_manager::GetTpmStatusReply tpm_status_;
  tpm_manager::ReadSpaceRequest last_read_space_request_;
  tpm_manager::ReadSpaceReply next_read_space_reply_;

  NiceMock<tpm_manager::MockTpmOwnershipInterface> mock_tpm_owner_;
  NiceMock<tpm_manager::MockTpmNvramInterface> mock_tpm_nvram_;
  NiceMock<trunks::MockTpm> mock_tpm_;
  NiceMock<trunks::MockTpmUtility> mock_tpm_utility_;
  trunks::TrunksFactoryForTest trunks_factory_for_test_;
  std::unique_ptr<TpmUtilityV2> tpm_utility_;
};

TEST_F(TpmUtilityTest, IsTpmReady) {
  EXPECT_TRUE(tpm_utility_->IsTpmReady());
}

TEST_F(TpmUtilityTest, IsTpmReadyNotOwned) {
  tpm_status_.set_owned(false);
  EXPECT_FALSE(tpm_utility_->IsTpmReady());
}

TEST_F(TpmUtilityTest, ActivateIdentity) {
  trunks::TPM2B_DIGEST fake_credential =
      trunks::Make_TPM2B_DIGEST("fake_credential");
  EXPECT_CALL(mock_tpm_, ActivateCredentialSync(_, _, _, _, _, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<6>(fake_credential), Return(TPM_RC_SUCCESS)));
  std::string credential;
  EXPECT_TRUE(tpm_utility_->ActivateIdentityForTpm2(
      KEY_TYPE_RSA, "fake_identity_blob", "seed", "mac", "wrapped",
      &credential));
  EXPECT_EQ("fake_credential", credential);
}

TEST_F(TpmUtilityTest, ActivateIdentityFailLoadIdentityKey) {
  EXPECT_CALL(mock_tpm_utility_, LoadKey(_, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_utility_, LoadKey("fake_identity_blob", _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  std::string credential;
  EXPECT_FALSE(tpm_utility_->ActivateIdentityForTpm2(
      KEY_TYPE_RSA, "fake_identity_blob", "seed", "mac", "wrapped",
      &credential));
  EXPECT_TRUE(credential.empty());
}

TEST_F(TpmUtilityTest, ActivateIdentityFailLoadEndorsementKey) {
  EXPECT_CALL(mock_tpm_utility_, GetEndorsementKey(_, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  std::string credential;
  EXPECT_FALSE(tpm_utility_->ActivateIdentityForTpm2(
      KEY_TYPE_RSA, "fake_identity_blob", "seed", "mac", "wrapped",
      &credential));
  EXPECT_TRUE(credential.empty());
}

TEST_F(TpmUtilityTest, ActivateIdentityNoEndorsementPassword) {
  tpm_status_.mutable_local_data()->clear_endorsement_password();
  std::string credential;
  EXPECT_FALSE(tpm_utility_->ActivateIdentityForTpm2(
      KEY_TYPE_RSA, "fake_identity_blob", "seed", "mac", "wrapped",
      &credential));
  EXPECT_TRUE(credential.empty());
}

TEST_F(TpmUtilityTest, ActivateIdentityError) {
  EXPECT_CALL(mock_tpm_, ActivateCredentialSync(_, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  std::string credential;
  EXPECT_FALSE(tpm_utility_->ActivateIdentityForTpm2(
      KEY_TYPE_RSA, "fake_identity_blob", "seed", "mac", "wrapped",
      &credential));
  EXPECT_TRUE(credential.empty());
}

TEST_F(TpmUtilityTest, GetEndorsementPublicKey) {
  std::string key;
  EXPECT_TRUE(tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_RSA, &key));
  EXPECT_TRUE(tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_ECC, &key));
}

TEST_F(TpmUtilityTest, GetEndorsementPublicKeyNoKey) {
  EXPECT_CALL(mock_tpm_utility_, GetEndorsementKey(_, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  std::string key;
  EXPECT_FALSE(tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_RSA, &key));
  EXPECT_TRUE(key.empty());
  EXPECT_FALSE(tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_ECC, &key));
  EXPECT_TRUE(key.empty());
}

TEST_F(TpmUtilityTest, GetEndorsementPublicKeyNoPublic) {
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(_, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  std::string key;
  EXPECT_FALSE(tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_RSA, &key));
  EXPECT_TRUE(key.empty());
  EXPECT_FALSE(tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_ECC, &key));
  EXPECT_TRUE(key.empty());
}

TEST_F(TpmUtilityTest, GetEndorsementCertificate) {
  std::string certificate;
  EXPECT_TRUE(
      tpm_utility_->GetEndorsementCertificate(KEY_TYPE_RSA, &certificate));
  EXPECT_TRUE(last_read_space_request_.has_index());
  last_read_space_request_.Clear();
  EXPECT_TRUE(
      tpm_utility_->GetEndorsementCertificate(KEY_TYPE_ECC, &certificate));
  EXPECT_TRUE(last_read_space_request_.has_index());
}

TEST_F(TpmUtilityTest, GetEndorsementCertificateNoCert) {
  next_read_space_reply_.set_result(
      tpm_manager::NVRAM_RESULT_SPACE_DOES_NOT_EXIST);
  std::string certificate;
  EXPECT_FALSE(
      tpm_utility_->GetEndorsementCertificate(KEY_TYPE_RSA, &certificate));
  EXPECT_TRUE(last_read_space_request_.has_index());
  last_read_space_request_.Clear();
  EXPECT_FALSE(
      tpm_utility_->GetEndorsementCertificate(KEY_TYPE_ECC, &certificate));
  EXPECT_TRUE(last_read_space_request_.has_index());
}

}  // namespace attestation
