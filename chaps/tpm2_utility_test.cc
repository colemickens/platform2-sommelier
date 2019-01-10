// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/tpm2_utility_impl.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <trunks/mock_hmac_session.h>
#include <trunks/mock_tpm.h>
#include <trunks/mock_tpm_state.h>
#include <trunks/mock_tpm_utility.h>
#include <trunks/trunks_factory_for_test.h>

#include "chaps/chaps_utility.h"

using brillo::SecureBlob;
using testing::_;
using testing::DoAll;
using testing::SetArgPointee;
using testing::NiceMock;
using testing::Return;
using trunks::kStorageRootKey;
using trunks::TPM_RC_FAILURE;
using trunks::TPM_RC_SUCCESS;

namespace chaps {

class TPM2UtilityTest : public testing::Test {
 public:
  TPM2UtilityTest() : factory_(new trunks::TrunksFactoryForTest()) {}
  ~TPM2UtilityTest() override {}

  void SetUp() override {
    factory_->set_tpm(&mock_tpm_);
    factory_->set_tpm_state(&mock_tpm_state_);
    factory_->set_tpm_utility(&mock_tpm_utility_);
    factory_->set_hmac_session(&mock_session_);
  }

  trunks::TPM2B_PUBLIC_KEY_RSA GetValidRSAPublicKey() {
    const char kValidModulus[] =
        "A1D50D088994000492B5F3ED8A9C5FC8772706219F4C063B2F6A8C6B74D3AD6B"
        "212A53D01DABB34A6261288540D420D3BA59ED279D859DE6227A7AB6BD88FADD"
        "FC3078D465F4DF97E03A52A587BD0165AE3B180FE7B255B7BEDC1BE81CB1383F"
        "E9E46F9312B1EF28F4025E7D332E33F4416525FEB8F0FC7B815E8FBB79CDABE6"
        "327B5A155FEF13F559A7086CB8A543D72AD6ECAEE2E704FF28824149D7F4E393"
        "D3C74E721ACA97F7ADBE2CCF7B4BCC165F7380F48065F2C8370F25F066091259"
        "D14EA362BAF236E3CD8771A94BDEDA3900577143A238AB92B6C55F11DEFAFB31"
        "7D1DC5B6AE210C52B008D87F2A7BFF6EB5C4FB32D6ECEC6505796173951A3167";
    std::vector<uint8_t> bytes;
    CHECK(base::HexStringToBytes(kValidModulus, &bytes));
    CHECK_EQ(bytes.size(), 256u);
    trunks::TPM2B_PUBLIC_KEY_RSA rsa;
    rsa.size = bytes.size();
    memcpy(rsa.buffer, bytes.data(), bytes.size());
    return rsa;
  }

 protected:
  std::unique_ptr<trunks::TrunksFactoryForTest> factory_;
  NiceMock<trunks::MockTpm> mock_tpm_;
  NiceMock<trunks::MockTpmState> mock_tpm_state_;
  NiceMock<trunks::MockTpmUtility> mock_tpm_utility_;
  NiceMock<trunks::MockHmacSession> mock_session_;
};


TEST(TPM2Utility_DeathTest, LoadKeyParentBadParent) {
  trunks::TrunksFactoryForTest factory;
  TPM2UtilityImpl utility(&factory);
  std::string key_blob;
  SecureBlob auth_data;
  int key_handle;
  int parent_handle = 42;
  EXPECT_DEATH_IF_SUPPORTED(utility.LoadKeyWithParent(1, key_blob, auth_data,
                                                      parent_handle,
                                                      &key_handle),
                            "Check failed");
}

TEST_F(TPM2UtilityTest, InitSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
    .WillOnce(Return(false));
  EXPECT_TRUE(utility.Init());
}

TEST_F(TPM2UtilityTest, InitTpmStateInitializationFail) {
  TPM2UtilityImpl utility(factory_.get());
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.Init());
}

TEST_F(TPM2UtilityTest, InitPlatformHierarchyEnabled) {
  TPM2UtilityImpl utility(factory_.get());
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
    .WillOnce(Return(true));
  EXPECT_FALSE(utility.Init());
}

TEST_F(TPM2UtilityTest, InitTpmNotOwned) {
  TPM2UtilityImpl utility(factory_.get());
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
    .WillOnce(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillOnce(Return(false));
  EXPECT_FALSE(utility.Init());
}

#ifndef CHAPS_TPM2_USE_PER_OP_SESSIONS
TEST_F(TPM2UtilityTest, InitTpmNoSession) {
  TPM2UtilityImpl utility(factory_.get());
  EXPECT_CALL(mock_tpm_state_, IsPlatformHierarchyEnabled())
    .WillOnce(Return(false));
  EXPECT_CALL(mock_session_, StartUnboundSession(true, true))
     .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.Init());
}
#endif

TEST_F(TPM2UtilityTest, IsTPMAvailable) {
  TPM2UtilityImpl utility(factory_.get());
  utility.is_enabled_ready_ = true;
  utility.is_enabled_ = true;
  EXPECT_TRUE(utility.IsTPMAvailable());

  utility.is_enabled_ready_ = true;
  utility.is_enabled_ = false;
  EXPECT_FALSE(utility.IsTPMAvailable());

  utility.is_initialized_ = true;
  utility.is_enabled_ready_ = false;
  EXPECT_TRUE(utility.IsTPMAvailable());
  EXPECT_EQ(utility.is_enabled_, true);
  EXPECT_EQ(utility.is_enabled_ready_, true);

  utility.is_initialized_ = false;
  utility.is_enabled_ready_ = false;
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.IsTPMAvailable());

  utility.is_initialized_ = false;
  utility.is_enabled_ready_ = false;
  EXPECT_CALL(mock_tpm_state_, Initialize())
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_state_, IsEnabled())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(utility.IsTPMAvailable());
  EXPECT_EQ(utility.is_enabled_, false);
  EXPECT_EQ(utility.is_enabled_ready_, true);
}

TEST_F(TPM2UtilityTest, AuthenticateSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  SecureBlob auth_data;
  SecureBlob new_master_key;
  std::string key_blob;
  std::string encrypted_master;
  EXPECT_TRUE(utility.Authenticate(1,
                                   auth_data,
                                   key_blob,
                                   encrypted_master,
                                   &new_master_key));
}

TEST_F(TPM2UtilityTest, AuthenticateLoadFail) {
  TPM2UtilityImpl utility(factory_.get());
  SecureBlob auth_data;
  SecureBlob new_master_key;
  std::string key_blob;
  std::string encrypted_master;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.Authenticate(1,
                                    auth_data,
                                    key_blob,
                                    encrypted_master,
                                    &new_master_key));
}


TEST_F(TPM2UtilityTest, AuthenticateUnbindFail) {
  TPM2UtilityImpl utility(factory_.get());
  SecureBlob auth_data;
  SecureBlob new_master_key;
  std::string key_blob;
  std::string encrypted_master;
  EXPECT_CALL(mock_tpm_utility_,
              AsymmetricDecrypt(_, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.Authenticate(1,
                                    auth_data,
                                    key_blob,
                                    encrypted_master,
                                    &new_master_key));
}

TEST_F(TPM2UtilityTest, ChangeAuthDataSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  SecureBlob old_auth;
  SecureBlob new_auth;
  std::string old_blob;
  std::string new_blob;
  EXPECT_CALL(mock_tpm_utility_, ChangeKeyAuthorizationData(_, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.ChangeAuthData(1,
                                     old_auth,
                                     new_auth,
                                     old_blob,
                                     &new_blob));
}

TEST_F(TPM2UtilityTest, ChangeAuthDataLoadFail) {
  TPM2UtilityImpl utility(factory_.get());
  SecureBlob old_auth;
  SecureBlob new_auth;
  std::string old_blob;
  std::string new_blob;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(_, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.ChangeAuthData(1,
                                      old_auth,
                                      new_auth,
                                      old_blob,
                                      &new_blob));
}

TEST_F(TPM2UtilityTest, ChangeAuthDataChangeAuthFail) {
  TPM2UtilityImpl utility(factory_.get());
  SecureBlob old_auth;
  SecureBlob new_auth;
  std::string old_blob;
  std::string new_blob;
  EXPECT_CALL(mock_tpm_utility_, ChangeKeyAuthorizationData(_, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.ChangeAuthData(1,
                                      old_auth,
                                      new_auth,
                                      old_blob,
                                      &new_blob));
}

TEST_F(TPM2UtilityTest, ChangeAuthDataFlushContextFail) {
  TPM2UtilityImpl utility(factory_.get());
  SecureBlob old_auth;
  SecureBlob new_auth;
  std::string old_blob;
  std::string new_blob;
  trunks::TPM_HANDLE key_handle = trunks::TPM_RH_FIRST;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(key_handle),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, FlushContextSync(key_handle, NULL))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.ChangeAuthData(1,
                                      old_auth,
                                      new_auth,
                                      old_blob,
                                      &new_blob));
}


TEST_F(TPM2UtilityTest, GenerateRandomSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  int num_bytes = 20;
  std::string random_data;
  std::string generated_data(20, 'a');
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, &random_data))
      .WillOnce(DoAll(SetArgPointee<2>(generated_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(utility.GenerateRandom(num_bytes, &random_data));
  EXPECT_EQ(random_data.size(), num_bytes);
}

TEST_F(TPM2UtilityTest, GenerateRandomFail) {
  TPM2UtilityImpl utility(factory_.get());
  int num_bytes = 20;
  std::string random_data;
  EXPECT_CALL(mock_tpm_utility_, GenerateRandom(num_bytes, _, &random_data))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.GenerateRandom(num_bytes, &random_data));
}

TEST_F(TPM2UtilityTest, StirRandomSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  std::string entropy_data;
  EXPECT_CALL(mock_tpm_utility_, StirRandom(entropy_data, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.StirRandom(entropy_data));
}

TEST_F(TPM2UtilityTest, StirRandomFail) {
  TPM2UtilityImpl utility(factory_.get());
  std::string entropy_data;
  EXPECT_CALL(mock_tpm_utility_, StirRandom(entropy_data, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.StirRandom(entropy_data));
}

TEST_F(TPM2UtilityTest, GenerateRSAKeySuccess) {
  TPM2UtilityImpl utility(factory_.get());
  int modulus_bits = 2048;
  std::string exponent("\x01\x00\x01", 3);
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_CALL(mock_tpm_utility_,
              CreateRSAKeyPair(_, modulus_bits, _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.GenerateRSAKey(1,
                                  modulus_bits,
                                  exponent,
                                  auth_data,
                                  &key_blob,
                                  &key_handle));
}

TEST_F(TPM2UtilityTest, GenerateRSAKeyWrongExponent) {
  TPM2UtilityImpl utility(factory_.get());
  int modulus_bits = 2048;
  std::string exponent(10, 'a');
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_FALSE(utility.GenerateRSAKey(1,
                                   modulus_bits,
                                   exponent,
                                   auth_data,
                                   &key_blob,
                                   &key_handle));
}

TEST_F(TPM2UtilityTest, GenerateRSAKeyModulusTooSmall) {
  TPM2UtilityImpl utility(factory_.get());
  int modulus_bits = 1;
  std::string exponent("\x01\x00\x01", 3);
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_FALSE(utility.GenerateRSAKey(1,
                                   modulus_bits,
                                   exponent,
                                   auth_data,
                                   &key_blob,
                                   &key_handle));
}

TEST_F(TPM2UtilityTest, GenerateRSAKeyCreateFail) {
  TPM2UtilityImpl utility(factory_.get());
  int modulus_bits = 2048;
  std::string exponent("\x01\x00\x01", 3);
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_CALL(mock_tpm_utility_, CreateRSAKeyPair(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.GenerateRSAKey(1,
                                   modulus_bits,
                                   exponent,
                                   auth_data,
                                   &key_blob,
                                   &key_handle));
}

TEST_F(TPM2UtilityTest, GenerateRSAKeyLoadFail) {
  TPM2UtilityImpl utility(factory_.get());
  int modulus_bits = 2048;
  std::string exponent("\x01\x00\x01", 3);
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.GenerateRSAKey(1,
                                   modulus_bits,
                                   exponent,
                                   auth_data,
                                   &key_blob,
                                   &key_handle));
}

TEST_F(TPM2UtilityTest, GetPublicKeySuccess) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = trunks::TPM_RH_FIRST;
  std::string exponent;
  std::string modulus;
  std::string test_modulus("test");
  trunks::TPMT_PUBLIC public_data;
  public_data.parameters.rsa_detail.exponent = 0x10001;
  public_data.unique.rsa = trunks::Make_TPM2B_PUBLIC_KEY_RSA(test_modulus);
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(utility.GetRSAPublicKey(key_handle, &exponent, &modulus));
  EXPECT_EQ(modulus.compare(test_modulus), 0);
}

TEST_F(TPM2UtilityTest, GetPublicKeyFail) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = trunks::TPM_RH_FIRST;
  std::string exponent;
  std::string modulus;
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.GetRSAPublicKey(key_handle, &exponent, &modulus));
}

TEST_F(TPM2UtilityTest, WrapKeySuccess) {
  TPM2UtilityImpl utility(factory_.get());
  std::string exponent("\x01\x00\x01", 3);
  std::string modulus(2048, 'a');
  std::string prime_factor;
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_CALL(mock_tpm_utility_,
              ImportRSAKey(_, modulus, _, prime_factor, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.WrapKey(1,
                              exponent,
                              modulus,
                              prime_factor,
                              auth_data,
                              &key_blob,
                              &key_handle));
}

TEST_F(TPM2UtilityTest, WrapKeyWrongExponent) {
  TPM2UtilityImpl utility(factory_.get());
  std::string exponent(10, 'a');
  std::string modulus(2048, 'a');
  std::string prime_factor;
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_FALSE(utility.WrapKey(1,
                               exponent,
                               modulus,
                               prime_factor,
                               auth_data,
                               &key_blob,
                               &key_handle));
}

TEST_F(TPM2UtilityTest, WrapKeyImportFail) {
  TPM2UtilityImpl utility(factory_.get());
  std::string exponent("\x01\x00\x01", 3);
  std::string modulus(2048, 'a');
  std::string prime_factor;
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_CALL(mock_tpm_utility_, ImportRSAKey(_, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.WrapKey(1,
                               exponent,
                               modulus,
                               prime_factor,
                               auth_data,
                               &key_blob,
                               &key_handle));
}

TEST_F(TPM2UtilityTest, WrapKeyLoadFail) {
  TPM2UtilityImpl utility(factory_.get());
  std::string exponent("\x01\x00\x01", 3);
  std::string modulus(2048, 'a');
  std::string prime_factor;
  SecureBlob auth_data;
  std::string key_blob;
  int key_handle;
  EXPECT_CALL(mock_tpm_utility_, ImportRSAKey(_, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.WrapKey(1,
                               exponent,
                               modulus,
                               prime_factor,
                               auth_data,
                               &key_blob,
                               &key_handle));
}

TEST_F(TPM2UtilityTest, LoadKeySuccess) {
  TPM2UtilityImpl utility(factory_.get());
  std::string key_blob;
  SecureBlob auth_data;
  int key_handle = 10;  // any value is acceptable.
  int slot = 1;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.LoadKey(slot, key_blob, auth_data, &key_handle));
  auto slot_set = utility.slot_handles_[slot];
  EXPECT_TRUE(slot_set.find(key_handle) != slot_set.end());
}

TEST_F(TPM2UtilityTest, LoadKeyFail) {
  TPM2UtilityImpl utility(factory_.get());
  std::string key_blob;
  SecureBlob auth_data;
  int key_handle;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.LoadKey(1, key_blob, auth_data, &key_handle));
}

TEST_F(TPM2UtilityTest, LoadKeyParentSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  std::string key_blob;
  SecureBlob auth_data;
  int key_handle;
  int parent_handle = kStorageRootKey;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.LoadKeyWithParent(1, key_blob, auth_data,
                                        parent_handle, &key_handle));
}

TEST_F(TPM2UtilityTest, LoadKeyParentLoadFail) {
  TPM2UtilityImpl utility(factory_.get());
  std::string key_blob;
  SecureBlob auth_data;
  int key_handle;
  int parent_handle = kStorageRootKey;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.LoadKeyWithParent(1, key_blob, auth_data,
                                         parent_handle, &key_handle));
}

TEST_F(TPM2UtilityTest, LoadKeyParentNameFail) {
  TPM2UtilityImpl utility(factory_.get());
  std::string key_blob;
  SecureBlob auth_data;
  int key_handle = 32;
  int parent_handle = kStorageRootKey;
  EXPECT_CALL(mock_tpm_utility_, LoadKey(key_blob, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_utility_, GetKeyName(key_handle, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.LoadKeyWithParent(1, key_blob, auth_data,
                                         parent_handle, &key_handle));
}

TEST_F(TPM2UtilityTest, UnloadKeysTest) {
  TPM2UtilityImpl utility(factory_.get());
  int slot1 = 1;
  int slot2 = 2;
  int key_handle1 = 1;
  int key_handle2 = 2;
  int key_handle3 = 3;
  utility.slot_handles_[slot1].insert(key_handle1);
  utility.slot_handles_[slot1].insert(key_handle2);
  utility.slot_handles_[slot2].insert(key_handle3);
  EXPECT_CALL(mock_tpm_, FlushContextSync(_, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_, FlushContextSync(key_handle1, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_, FlushContextSync(key_handle2, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  utility.UnloadKeysForSlot(slot1);
  auto slot1_set = utility.slot_handles_[slot1];
  auto slot2_set = utility.slot_handles_[slot2];
  EXPECT_TRUE(slot1_set.find(key_handle1) == slot1_set.end());
  EXPECT_TRUE(slot1_set.find(key_handle2) == slot1_set.end());
  EXPECT_TRUE(slot2_set.find(key_handle3) != slot2_set.end());
}

TEST_F(TPM2UtilityTest, BindSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input("input");
  std::string output;

  trunks::TPMT_PUBLIC public_data;
  public_data.parameters.rsa_detail.exponent = 0x10001;
  public_data.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_TRUE(utility.Bind(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, UnbindSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input;
  std::string output;
  EXPECT_CALL(mock_tpm_utility_,
              AsymmetricDecrypt(key_handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.Unbind(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, UnbindFailure) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input;
  std::string output;
  EXPECT_CALL(mock_tpm_utility_,
              AsymmetricDecrypt(key_handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.Unbind(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, SignSuccess) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input = GetDigestAlgorithmEncoding(DigestAlgorithm::SHA1);
  std::string output;
  trunks::TPMT_PUBLIC public_data;
  public_data.parameters.rsa_detail.exponent = 0x10001;
  public_data.object_attributes = trunks::kSign;
  public_data.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, Sign(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.Sign(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, SignSuccessWithDecrypt) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input = GetDigestAlgorithmEncoding(DigestAlgorithm::SHA1);
  std::string output;
  trunks::TPMT_PUBLIC public_data;
  public_data.parameters.rsa_detail.exponent = 0x10001;
  public_data.object_attributes = trunks::kSign | trunks::kDecrypt;
  public_data.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, AsymmetricDecrypt(key_handle, _, _, _,  _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.Sign(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, SignFailure) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input = GetDigestAlgorithmEncoding(DigestAlgorithm::SHA1);
  std::string output;
  trunks::TPMT_PUBLIC public_data;
  public_data.parameters.rsa_detail.exponent = 0x10001;
  public_data.object_attributes = trunks::kSign;
  public_data.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, Sign(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.Sign(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, SignFailureWithDecrypt) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input = GetDigestAlgorithmEncoding(DigestAlgorithm::SHA1);
  std::string output;
  trunks::TPMT_PUBLIC public_data;
  public_data.parameters.rsa_detail.exponent = 0x10001;
  public_data.object_attributes = trunks::kSign | trunks::kDecrypt;
  public_data.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, AsymmetricDecrypt(key_handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_FALSE(utility.Sign(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, SignFailureBadKeySize) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input;
  std::string output;
  trunks::TPMT_PUBLIC public_data = {};
  public_data.object_attributes = trunks::kSign | trunks::kDecrypt;
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, Sign(key_handle, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_tpm_utility_, AsymmetricDecrypt(key_handle, _, _, _, _, _))
      .Times(0);
  EXPECT_FALSE(utility.Sign(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, SignFailurePublicArea) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input;
  std::string output;
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_CALL(mock_tpm_utility_, Sign(key_handle, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_tpm_utility_, AsymmetricDecrypt(key_handle, _, _, _, _, _))
      .Times(0);
  EXPECT_FALSE(utility.Sign(key_handle, input, &output));
}

TEST_F(TPM2UtilityTest, SignSuccessWithUnknownAlgorithm) {
  TPM2UtilityImpl utility(factory_.get());
  int key_handle = 43;
  std::string input = "test";
  std::string output;
  trunks::TPMT_PUBLIC public_data;
  public_data.parameters.rsa_detail.exponent = 0x10001;
  public_data.object_attributes = trunks::kSign;
  public_data.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_utility_, GetKeyPublicArea(key_handle, _))
      .WillOnce(DoAll(SetArgPointee<1>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_utility_, Sign(key_handle, trunks::TPM_ALG_RSASSA,
                                      trunks::TPM_ALG_NULL, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_TRUE(utility.Sign(key_handle, input, &output));
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
