// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stl_util.h>
#include <crypto/sha2.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/aes.h>

#include "trunks/error_codes.h"
#include "trunks/hmac_authorization_delegate.h"
#include "trunks/mock_authorization_delegate.h"
#include "trunks/mock_authorization_session.h"
#include "trunks/mock_tpm.h"
#include "trunks/mock_tpm_state.h"
#include "trunks/tpm_constants.h"
#include "trunks/tpm_utility_impl.h"
#include "trunks/trunks_factory_for_test.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
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
    factory_.set_authorization_session(&mock_authorization_session_);
  }
 protected:
  TrunksFactoryForTest factory_;
  NiceMock<MockTpmState> mock_tpm_state_;
  NiceMock<MockTpm> mock_tpm_;
  NiceMock<MockAuthorizationSession> mock_authorization_session_;
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

TEST_F(TpmUtilityTest, ClearSuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, ClearSync(_, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Clear());
}

TEST_F(TpmUtilityTest, ClearAfterBadInit) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, ClearSync(_, _, _))
      .WillOnce(Return(TPM_RC_AUTH_MISSING))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Clear());
}

TEST_F(TpmUtilityTest, ClearFail) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, ClearSync(_, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.Clear());
}

TEST_F(TpmUtilityTest, ShutdownTest) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, ShutdownSync(TPM_SU_CLEAR, _));
  utility.Shutdown();
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
  // Reject attempts to disable the platform hierarchy.
  EXPECT_CALL(mock_tpm_, HierarchyControlSync(_, _, TPM_RH_PLATFORM, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.InitializeTpm());
}

TEST_F(TpmUtilityTest, TakeOwnershipSuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsEndorsementPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsLockoutPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_OWNER, _, _, _))
      .Times(2);
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_ENDORSEMENT, _, _, _))
      .Times(1);
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_LOCKOUT, _, _, _))
      .Times(1);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.TakeOwnership("a", "b", "c"));
}

TEST_F(TpmUtilityTest, TakeOwnershipAlreadyDone) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_state_, IsEndorsementPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_state_, IsLockoutPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(_, _, _, _))
      .Times(1);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.TakeOwnership("a", "b", "c"));
}

TEST_F(TpmUtilityTest, TakeOwnershipPartial) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_state_, IsEndorsementPasswordSet())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsLockoutPasswordSet())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_OWNER, _, _, _))
      .Times(1);
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_ENDORSEMENT, _, _, _))
      .Times(1);
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_LOCKOUT, _, _, _))
      .Times(0);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.TakeOwnership("a", "b", "c"));
}

TEST_F(TpmUtilityTest, TakeOwnershipOwnerFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsEndorsementPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsLockoutPasswordSet())
      .WillRepeatedly(Return(false));
  // Reject attempts to set owner auth.
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_OWNER, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_ENDORSEMENT, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_LOCKOUT, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_FAILURE, utility.TakeOwnership("a", "b", "c"));
}

TEST_F(TpmUtilityTest, TakeOwnershipEndorsementFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsEndorsementPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsLockoutPasswordSet())
      .WillRepeatedly(Return(false));
  // Reject attempts to set endorsement auth.
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_OWNER, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_ENDORSEMENT, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_LOCKOUT, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_FAILURE, utility.TakeOwnership("a", "b", "c"));
}

TEST_F(TpmUtilityTest, TakeOwnershipLockoutFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsEndorsementPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsLockoutPasswordSet())
      .WillRepeatedly(Return(false));
  // Reject attempts to set lockout auth.
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_OWNER, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_ENDORSEMENT, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_LOCKOUT, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.TakeOwnership("a", "b", "c"));
}

TEST_F(TpmUtilityTest, StirRandomSuccess) {
  TpmUtilityImpl utility(factory_);
  std::string entropy_data("large test data", 100);
  NiceMock<MockAuthorizationDelegate> delegate;
  EXPECT_CALL(mock_authorization_session_, GetDelegate())
      .WillOnce(Return(&delegate));
  EXPECT_CALL(mock_tpm_, StirRandomSync(_, &delegate))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS,
            utility.StirRandom(entropy_data, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, StirRandomFails) {
  TpmUtilityImpl utility(factory_);
  std::string entropy_data("test data");
  EXPECT_CALL(mock_tpm_, StirRandomSync(_, NULL))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.StirRandom(entropy_data, NULL));
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
  NiceMock<MockAuthorizationDelegate> delegate;
  EXPECT_CALL(mock_authorization_session_, GetDelegate())
      .WillOnce(Return(&delegate));
  EXPECT_CALL(mock_tpm_, GetRandomSync(_, _, &delegate))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<1>(large_random),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, GetRandomSync(8, _, &delegate))
      .WillOnce(DoAll(SetArgPointee<1>(small_random),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.GenerateRandom(num_bytes,
                                                   &mock_authorization_session_,
                                                   &random_data));
  EXPECT_EQ(num_bytes, random_data.size());
}

TEST_F(TpmUtilityTest, GenerateRandomFails) {
  TpmUtilityImpl utility(factory_);
  int num_bytes = 5;
  std::string random_data;
  EXPECT_CALL(mock_tpm_, GetRandomSync(_, _, NULL))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE,
            utility.GenerateRandom(num_bytes, NULL, &random_data));
}

TEST_F(TpmUtilityTest, ExtendPCRSuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE pcr_handle = HR_PCR + 1;
  NiceMock<MockAuthorizationDelegate> delegate;
  EXPECT_CALL(mock_authorization_session_, GetDelegate())
      .WillOnce(Return(&delegate));
  EXPECT_CALL(mock_tpm_, PCR_ExtendSync(pcr_handle, _, _, &delegate))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS,
            utility.ExtendPCR(1, "test digest", &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, ExtendPCRFail) {
  TpmUtilityImpl utility(factory_);
  int pcr_index = 0;
  TPM_HANDLE pcr_handle = HR_PCR + pcr_index;
  EXPECT_CALL(mock_tpm_, PCR_ExtendSync(pcr_handle, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ExtendPCR(pcr_index, "test digest", NULL));
}

TEST_F(TpmUtilityTest, ExtendPCRBadParam) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_FAILURE, utility.ExtendPCR(-1, "test digest", NULL));
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
  pcr_values.digests[0].size = 5;
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

TEST_F(TpmUtilityTest, AsymmetricEncryptSuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string output_ciphertext("ciphertext");
  std::string ciphertext;
  TPM2B_PUBLIC_KEY_RSA out_message = Make_TPM2B_PUBLIC_KEY_RSA(
      output_ciphertext);
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  NiceMock<MockAuthorizationDelegate> delegate;
  EXPECT_CALL(mock_authorization_session_, GetDelegate())
      .WillOnce(Return(&delegate));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, &delegate))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricEncrypt(
      key_handle,
      TPM_ALG_NULL,
      TPM_ALG_NULL,
      plaintext,
      &mock_authorization_session_,
      &ciphertext));
  EXPECT_EQ(0, ciphertext.compare(output_ciphertext));
}

TEST_F(TpmUtilityTest, AsymmetricEncryptFail) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string ciphertext;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, NULL))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.AsymmetricEncrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      plaintext,
                                                      NULL,
                                                      &ciphertext));
}

TEST_F(TpmUtilityTest, AsymmetricEncryptBadParams) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string ciphertext;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt | kRestricted;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, NULL))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.AsymmetricEncrypt(key_handle,
                                                             TPM_ALG_RSAES,
                                                             TPM_ALG_NULL,
                                                             plaintext,
                                                             NULL,
                                                             &ciphertext));
}

TEST_F(TpmUtilityTest, AsymmetricEncryptNullSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string output_ciphertext("ciphertext");
  std::string ciphertext;
  TPM2B_PUBLIC_KEY_RSA out_message = Make_TPM2B_PUBLIC_KEY_RSA(
      output_ciphertext);
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, NULL))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricEncrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      plaintext,
                                                      NULL,
                                                      &ciphertext));
  EXPECT_EQ(scheme.scheme, TPM_ALG_OAEP);
  EXPECT_EQ(scheme.details.oaep.hash_alg, TPM_ALG_SHA256);
}

TEST_F(TpmUtilityTest, AsymmetricEncryptSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string output_ciphertext("ciphertext");
  std::string ciphertext;
  TPM2B_PUBLIC_KEY_RSA out_message = Make_TPM2B_PUBLIC_KEY_RSA(
      output_ciphertext);
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, NULL))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricEncrypt(key_handle,
                                                      TPM_ALG_RSAES,
                                                      TPM_ALG_NULL,
                                                      plaintext,
                                                      NULL,
                                                      &ciphertext));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSAES);
}

TEST_F(TpmUtilityTest, AsymmetricDecryptSuccess) {
  TpmUtilityImpl utility(factory_);
  scoped_ptr<AuthorizationSession> session(factory_.GetAuthorizationSession());
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string output_plaintext("plaintext");
  std::string ciphertext;
  std::string password("password");
  TPM2B_PUBLIC_KEY_RSA out_message = Make_TPM2B_PUBLIC_KEY_RSA(
      output_plaintext);
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricDecrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      ciphertext,
                                                      session.get(),
                                                      &plaintext));
  EXPECT_EQ(0, plaintext.compare(output_plaintext));
}

TEST_F(TpmUtilityTest, AsymmetricDecryptFail) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string key_name;
  std::string plaintext;
  std::string ciphertext;
  std::string password;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.AsymmetricDecrypt(
      key_handle,
      TPM_ALG_NULL,
      TPM_ALG_NULL,
      ciphertext,
      &mock_authorization_session_,
      &plaintext));
}

TEST_F(TpmUtilityTest, AsymmetricDecryptBadParams) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string key_name;
  std::string plaintext;
  std::string ciphertext;
  std::string password;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt | kRestricted;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.AsymmetricDecrypt(
      key_handle,
      TPM_ALG_RSAES,
      TPM_ALG_NULL,
      ciphertext,
      &mock_authorization_session_,
      &plaintext));
}

TEST_F(TpmUtilityTest, AsymmetricDecryptBadSession) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = TPM_RH_FIRST;
  std::string key_name;
  std::string plaintext;
  std::string ciphertext;
  std::string password;
  EXPECT_EQ(SAPI_RC_INVALID_SESSIONS, utility.AsymmetricDecrypt(
      key_handle, TPM_ALG_RSAES, TPM_ALG_NULL, ciphertext, NULL, &plaintext));
}

TEST_F(TpmUtilityTest, AsymmetricDecryptNullSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string output_plaintext("plaintext");
  std::string ciphertext;
  std::string password;
  TPM2B_PUBLIC_KEY_RSA out_message = Make_TPM2B_PUBLIC_KEY_RSA(
      output_plaintext);
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricDecrypt(
      key_handle,
      TPM_ALG_NULL,
      TPM_ALG_NULL,
      ciphertext,
      &mock_authorization_session_,
      &plaintext));
  EXPECT_EQ(scheme.scheme, TPM_ALG_OAEP);
  EXPECT_EQ(scheme.details.oaep.hash_alg, TPM_ALG_SHA256);
}

TEST_F(TpmUtilityTest, AsymmetricDecryptSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string plaintext;
  std::string output_plaintext("plaintext");
  std::string ciphertext;
  std::string password;
  TPM2B_PUBLIC_KEY_RSA out_message = Make_TPM2B_PUBLIC_KEY_RSA(
      output_plaintext);
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricDecrypt(
      key_handle,
      TPM_ALG_RSAES,
      TPM_ALG_NULL,
      ciphertext,
      &mock_authorization_session_,
      &plaintext));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSAES);
}

TEST_F(TpmUtilityTest, SignSuccess) {
  TpmUtilityImpl utility(factory_);
  scoped_ptr<AuthorizationSession> session(factory_.GetAuthorizationSession());
  TPM_HANDLE key_handle;
  std::string password("password");
  std::string digest(32, 'a');
  TPMT_SIGNATURE signature_out;
  signature_out.signature.rsassa.sig.size = 2;
  signature_out.signature.rsassa.sig.buffer[0] = 'h';
  signature_out.signature.rsassa.sig.buffer[1] = 'i';
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, SignSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(signature_out),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Sign(key_handle,
                                         TPM_ALG_NULL,
                                         TPM_ALG_NULL,
                                         digest,
                                         session.get(),
                                         &signature));
  EXPECT_EQ(0, signature.compare("hi"));
}

TEST_F(TpmUtilityTest, SignFail) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, SignSync(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.Sign(key_handle,
                                         TPM_ALG_NULL,
                                         TPM_ALG_NULL,
                                         digest,
                                         &mock_authorization_session_,
                                         &signature));
}

TEST_F(TpmUtilityTest, SignBadParams1) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign | kRestricted;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_RSAPSS,
                                                TPM_ALG_NULL,
                                                digest,
                                                &mock_authorization_session_,
                                                &signature));
}

TEST_F(TpmUtilityTest, SignBadAuthorizationSession) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = TPM_RH_FIRST;
  std::string password;
  std::string digest(32, 'a');
  std::string signature;
  EXPECT_EQ(SAPI_RC_INVALID_SESSIONS, utility.Sign(key_handle,
                                                   TPM_ALG_RSAPSS,
                                                   TPM_ALG_NULL,
                                                   digest,
                                                   NULL,
                                                   &signature));
}

TEST_F(TpmUtilityTest, SignBadParams2) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_RSAPSS,
                                                TPM_ALG_NULL,
                                                digest,
                                                &mock_authorization_session_,
                                                &signature));
}

TEST_F(TpmUtilityTest, SignBadParams3) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_ECC;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_RSAPSS,
                                                TPM_ALG_NULL,
                                                digest,
                                                &mock_authorization_session_,
                                                &signature));
}

TEST_F(TpmUtilityTest, SignBadParams4) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_FAILURE)));
  EXPECT_EQ(TPM_RC_FAILURE, utility.Sign(key_handle,
                                         TPM_ALG_RSAPSS,
                                         TPM_ALG_NULL,
                                         digest,
                                         &mock_authorization_session_,
                                         &signature));
}

TEST_F(TpmUtilityTest, SignBadParams5) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = 0;
  std::string password;
  std::string digest(32, 'a');
  std::string signature;
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_AES,
                                                TPM_ALG_NULL,
                                                digest,
                                                &mock_authorization_session_,
                                                &signature));
}


TEST_F(TpmUtilityTest, SignNullSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest(32, 'a');
  TPMT_SIGNATURE signature_out;
  signature_out.signature.rsassa.sig.size = 0;
  std::string signature;
  TPM2B_PUBLIC public_area;
  TPMT_SIG_SCHEME scheme;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, SignSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(signature_out),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Sign(key_handle,
                                         TPM_ALG_NULL,
                                         TPM_ALG_NULL,
                                         digest,
                                         &mock_authorization_session_,
                                         &signature));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSASSA);
  EXPECT_EQ(scheme.details.rsassa.hash_alg, TPM_ALG_SHA256);
}

TEST_F(TpmUtilityTest, SignSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest(64, 'a');
  TPMT_SIGNATURE signature_out;
  signature_out.signature.rsassa.sig.size = 0;
  std::string signature;
  TPM2B_PUBLIC public_area;
  TPMT_SIG_SCHEME scheme;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, SignSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(signature_out),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Sign(key_handle,
                                         TPM_ALG_RSAPSS,
                                         TPM_ALG_SHA1,
                                         digest,
                                         &mock_authorization_session_,
                                         &signature));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSAPSS);
  EXPECT_EQ(scheme.details.rsapss.hash_alg, TPM_ALG_SHA1);
}

TEST_F(TpmUtilityTest, VerifySuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, VerifySignatureSync(key_handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Verify(key_handle,
                                           TPM_ALG_NULL,
                                           TPM_ALG_NULL,
                                           digest,
                                           signature));
}

TEST_F(TpmUtilityTest, VerifyFail) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, VerifySignatureSync(key_handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.Verify(key_handle,
                                           TPM_ALG_NULL,
                                           TPM_ALG_NULL,
                                           digest,
                                           signature));
}

TEST_F(TpmUtilityTest, VerifyBadParams1) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign | kRestricted;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Verify(key_handle,
                                                  TPM_ALG_NULL,
                                                  TPM_ALG_NULL,
                                                  digest,
                                                  signature));
}

TEST_F(TpmUtilityTest, VerifyBadParams2) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Verify(key_handle,
                                                  TPM_ALG_NULL,
                                                  TPM_ALG_NULL,
                                                  digest,
                                                  signature));
}

TEST_F(TpmUtilityTest, VerifyBadParams3) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_ECC;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Verify(key_handle,
                                                  TPM_ALG_NULL,
                                                  TPM_ALG_NULL,
                                                  digest,
                                                  signature));
}

TEST_F(TpmUtilityTest, VerifyBadParams4) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_FAILURE)));
  EXPECT_EQ(TPM_RC_FAILURE, utility.Verify(key_handle,
                                           TPM_ALG_NULL,
                                           TPM_ALG_NULL,
                                           digest,
                                           signature));
}

TEST_F(TpmUtilityTest, VerifyBadParams5) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Verify(key_handle,
                                                  TPM_ALG_AES,
                                                  TPM_ALG_NULL,
                                                  digest,
                                                  signature));
}

TEST_F(TpmUtilityTest, VerifyNullSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(32, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  TPMT_SIGNATURE signature_in;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, VerifySignatureSync(key_handle, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&signature_in),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Verify(key_handle,
                                           TPM_ALG_NULL,
                                           TPM_ALG_NULL,
                                           digest,
                                           signature));
  EXPECT_EQ(signature_in.sig_alg, TPM_ALG_RSASSA);
  EXPECT_EQ(signature_in.signature.rsassa.hash, TPM_ALG_SHA256);
}

TEST_F(TpmUtilityTest, VerifySchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest(64, 'a');
  std::string signature;
  TPM2B_PUBLIC public_area;
  TPMT_SIGNATURE signature_in;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, VerifySignatureSync(key_handle, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&signature_in),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Verify(key_handle,
                                           TPM_ALG_RSAPSS,
                                           TPM_ALG_SHA1,
                                           digest,
                                           signature));
  EXPECT_EQ(signature_in.sig_alg, TPM_ALG_RSAPSS);
  EXPECT_EQ(signature_in.signature.rsassa.hash, TPM_ALG_SHA1);
}

TEST_F(TpmUtilityTest, ChangeAuthDataSuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = 1;
  std::string new_password;
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ChangeKeyAuthorizationData(
    key_handle, new_password, &mock_authorization_session_, NULL));
}

TEST_F(TpmUtilityTest, ChangeAuthDataKeyNameFail) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = 1;
  std::string old_password;
  std::string new_password;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ChangeKeyAuthorizationData(
      key_handle, new_password, &mock_authorization_session_, NULL));
}

TEST_F(TpmUtilityTest, ChangeAuthDataFailure) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = 1;
  std::string new_password;
  EXPECT_CALL(mock_tpm_, ObjectChangeAuthSync(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ChangeKeyAuthorizationData(
      key_handle, new_password, &mock_authorization_session_, NULL));
}

TEST_F(TpmUtilityTest, ChangeAuthDataWithReturnSuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = 1;
  std::string new_password;
  std::string key_blob;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.auth_policy.size = 0;
  public_area.public_area.unique.rsa.size = 0;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(_, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ChangeKeyAuthorizationData(
    key_handle, new_password, &mock_authorization_session_, &key_blob));
}

TEST_F(TpmUtilityTest, ImportRSAKeySuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t public_exponent = 0x10001;
  std::string modulus(256, 'a');
  std::string prime_factor(128, 'b');
  std::string password("password");
  std::string key_blob;
  TPM2B_DATA encryption_key;
  TPM2B_PUBLIC public_data;
  TPM2B_PRIVATE private_data;
  EXPECT_CALL(mock_tpm_, ImportSync(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&encryption_key),
                      SaveArg<3>(&public_data),
                      SaveArg<4>(&private_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ImportRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      modulus,
      public_exponent,
      prime_factor,
      password,
      &mock_authorization_session_,
      &key_blob));
  // Validate that the public area was properly constructed.
  EXPECT_EQ(public_data.public_area.parameters.rsa_detail.key_bits,
            modulus.size() * 8);
  EXPECT_EQ(public_data.public_area.parameters.rsa_detail.exponent,
            public_exponent);
  EXPECT_EQ(public_data.public_area.unique.rsa.size, modulus.size());
  EXPECT_EQ(0, memcmp(public_data.public_area.unique.rsa.buffer,
                      modulus.data(), modulus.size()));
  // Validate the private struct construction.
  EXPECT_EQ(kAesKeySize, encryption_key.size);
  AES_KEY key;
  AES_set_encrypt_key(encryption_key.buffer, kAesKeySize * 8, &key);
  unsigned char iv[MAX_AES_BLOCK_SIZE_BYTES] = {0};
  int iv_in = 0;
  std::string unencrypted_private(private_data.size, 0);
  AES_cfb128_encrypt(
    reinterpret_cast<const unsigned char*>(private_data.buffer),
    reinterpret_cast<unsigned char*>(string_as_array(&unencrypted_private)),
    private_data.size, &key, iv, &iv_in, AES_DECRYPT);
  TPM2B_DIGEST inner_integrity;
  EXPECT_EQ(TPM_RC_SUCCESS, Parse_TPM2B_DIGEST(&unencrypted_private,
                                               &inner_integrity, NULL));
  std::string object_name;
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ComputeKeyName(public_data.public_area,
                                                   &object_name));
  std::string integrity_value = crypto::SHA256HashString(unencrypted_private +
                                                         object_name);
  EXPECT_EQ(integrity_value.size(), inner_integrity.size);
  EXPECT_EQ(0, memcmp(inner_integrity.buffer,
                      integrity_value.data(),
                      inner_integrity.size));
  TPM2B_SENSITIVE sensitive_data;
  EXPECT_EQ(TPM_RC_SUCCESS, Parse_TPM2B_SENSITIVE(&unencrypted_private,
                                                  &sensitive_data, NULL));
  EXPECT_EQ(sensitive_data.sensitive_area.auth_value.size, password.size());
  EXPECT_EQ(0, memcmp(sensitive_data.sensitive_area.auth_value.buffer,
                      password.data(), password.size()));
  EXPECT_EQ(sensitive_data.sensitive_area.sensitive.rsa.size,
            prime_factor.size());
  EXPECT_EQ(0, memcmp(sensitive_data.sensitive_area.sensitive.rsa.buffer,
                      prime_factor.data(), prime_factor.size()));
}

TEST_F(TpmUtilityTest, ImportRSAKeySuccessWithNoBlob) {
  TpmUtilityImpl utility(factory_);
  uint32_t public_exponent = 0x10001;
  std::string modulus(256, 'a');
  std::string prime_factor(128, 'b');
  std::string password;
  EXPECT_CALL(mock_tpm_, ImportSync(_, _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ImportRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      modulus,
      public_exponent,
      prime_factor,
      password,
      &mock_authorization_session_,
      NULL));
}

TEST_F(TpmUtilityTest, ImportRSAKeyParentNameFail) {
  TpmUtilityImpl utility(factory_);
  uint32_t public_exponent = 0x10001;
  std::string modulus(256, 'a');
  std::string prime_factor(128, 'b');
  std::string password;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(_, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ImportRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      modulus,
      public_exponent,
      prime_factor,
      password,
      &mock_authorization_session_,
      NULL));
}

TEST_F(TpmUtilityTest, ImportRSAKeyFail) {
    TpmUtilityImpl utility(factory_);
  std::string modulus;
  std::string prime_factor;
  std::string password;
  EXPECT_CALL(mock_tpm_, ImportSync(_, _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ImportRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      modulus,
      0x10001,
      prime_factor,
      password,
      &mock_authorization_session_,
      NULL));
}

TEST_F(TpmUtilityTest, CreateAndLoadRSAKeyDecryptSuccess) {
  TpmUtilityImpl utility(factory_);
  scoped_ptr<AuthorizationSession> session(factory_.GetAuthorizationSession());
  TPM_HANDLE key_handle;
  TPM2B_PUBLIC public_area;
  EXPECT_CALL(mock_tpm_, CreateSyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&public_area),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, LoadSync(_, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.CreateAndLoadRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      "password",
      session.get(),
      &key_handle,
      NULL));
  EXPECT_EQ(public_area.public_area.object_attributes & kDecrypt, kDecrypt);
  EXPECT_EQ(public_area.public_area.object_attributes & kSign, 0);
  EXPECT_EQ(public_area.public_area.parameters.rsa_detail.scheme.scheme,
            TPM_ALG_NULL);
}

TEST_F(TpmUtilityTest, CreateAndLoadRSAKeySignSuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  TPM2B_PUBLIC public_area;
  EXPECT_CALL(mock_tpm_, CreateSyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&public_area),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, LoadSync(_, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.CreateAndLoadRSAKey(
      TpmUtility::AsymmetricKeyUsage::kSignKey,
      "password",
      &mock_authorization_session_,
      &key_handle,
      NULL));
  EXPECT_EQ(public_area.public_area.object_attributes & kSign, kSign);
  EXPECT_EQ(public_area.public_area.object_attributes & kDecrypt, 0);
  EXPECT_EQ(public_area.public_area.parameters.rsa_detail.scheme.scheme,
            TPM_ALG_NULL);
}

TEST_F(TpmUtilityTest, CreateAndLoadRSAKeyLegacySuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  TPM2B_PUBLIC public_area;
  EXPECT_CALL(mock_tpm_, CreateSyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&public_area),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, LoadSync(_, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.CreateAndLoadRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptAndSignKey,
      "password",
      &mock_authorization_session_,
      &key_handle,
      NULL));
  EXPECT_EQ(public_area.public_area.object_attributes & kDecrypt, kDecrypt);
  EXPECT_EQ(public_area.public_area.object_attributes & kSign, kSign);
  EXPECT_EQ(public_area.public_area.parameters.rsa_detail.scheme.scheme,
            TPM_ALG_NULL);
}

TEST_F(TpmUtilityTest, CreateAndLoadRSAKeyFail1) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  EXPECT_CALL(mock_tpm_, CreateSyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.CreateAndLoadRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      "password",
      &mock_authorization_session_,
      &key_handle,
      NULL));
}

TEST_F(TpmUtilityTest, CreateAndLoadRSAKeyFail2) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  EXPECT_CALL(mock_tpm_, CreateSyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_CALL(mock_tpm_, LoadSync(_, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.CreateAndLoadRSAKey(
      TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      "password",
      &mock_authorization_session_,
      &key_handle,
      NULL));
}

TEST_F(TpmUtilityTest, DefineNVSpaceSuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 59;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  size_t length  = 256;
  TPM2B_NV_PUBLIC public_data;
  EXPECT_CALL(mock_tpm_, NV_DefineSpaceSync(TPM_RH_OWNER, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS,
            utility.DefineNVSpace(index, length, &mock_authorization_session_));
  EXPECT_EQ(public_data.nv_public.nv_index, nvram_index);
  EXPECT_EQ(public_data.nv_public.name_alg, TPM_ALG_SHA256);
  EXPECT_EQ(public_data.nv_public.attributes,
            TPMA_NV_OWNERWRITE | TPMA_NV_WRITEDEFINE | TPMA_NV_AUTHREAD);
  EXPECT_EQ(public_data.nv_public.data_size, length);
}

TEST_F(TpmUtilityTest, DefineNVSpaceBadLength) {
  TpmUtilityImpl utility(factory_);
  size_t bad_length = 3000;
  EXPECT_EQ(SAPI_RC_BAD_SIZE,
            utility.DefineNVSpace(0, bad_length, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, DefineNVSpaceBadIndex) {
  TpmUtilityImpl utility(factory_);
  uint32_t bad_index = 1<<29;
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER,
            utility.DefineNVSpace(bad_index, 2, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, DefineNVSpaceBadSession) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(SAPI_RC_INVALID_SESSIONS, utility.DefineNVSpace(0, 2, NULL));
}

TEST_F(TpmUtilityTest, DefineNVSpaceFail) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 59;
  size_t length  = 256;
  EXPECT_CALL(mock_tpm_, NV_DefineSpaceSync(TPM_RH_OWNER, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE,
            utility.DefineNVSpace(index, length, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, DestroyNVSpaceSuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  EXPECT_CALL(mock_tpm_,
              NV_UndefineSpaceSync(TPM_RH_OWNER, _, nvram_index, _, _));
  EXPECT_EQ(TPM_RC_SUCCESS,
            utility.DestroyNVSpace(index, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, DestroyNVSpaceBadIndex) {
  TpmUtilityImpl utility(factory_);
  uint32_t bad_index = 1<<29;
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER,
            utility.DestroyNVSpace(bad_index, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, DestroyNVSpaceBadSession) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(SAPI_RC_INVALID_SESSIONS, utility.DestroyNVSpace(3, NULL));
}

TEST_F(TpmUtilityTest, DestroyNVSpaceFailure) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  EXPECT_CALL(mock_tpm_,
              NV_UndefineSpaceSync(TPM_RH_OWNER, _, nvram_index, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE,
            utility.DestroyNVSpace(index, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, LockNVSpaceSuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  EXPECT_CALL(mock_tpm_, NV_WriteLockSync(nvram_index, _, nvram_index, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS,
            utility.LockNVSpace(index, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, LockNVSpaceBadIndex) {
  TpmUtilityImpl utility(factory_);
  uint32_t bad_index = 1<<24;
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER,
            utility.LockNVSpace(bad_index, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, LockNVSpaceBadSession) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(SAPI_RC_INVALID_SESSIONS, utility.LockNVSpace(52, NULL));
}

TEST_F(TpmUtilityTest, LockNVSpaceFailure) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  EXPECT_CALL(mock_tpm_, NV_WriteLockSync(nvram_index, _, nvram_index, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE,
            utility.LockNVSpace(index, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, WriteNVSpaceSuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t offset = 5;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  EXPECT_CALL(mock_tpm_,
              NV_WriteSync(TPM_RH_OWNER, _, nvram_index, _, _, offset, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.WriteNVSpace(
      index, offset, "", &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, WriteNVSpaceBadSize) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  std::string nvram_data(1025, 0);
  EXPECT_EQ(SAPI_RC_BAD_SIZE, utility.WriteNVSpace(
      index, 0, nvram_data, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, WriteNVSpaceBadIndex) {
  TpmUtilityImpl utility(factory_);
  uint32_t bad_index = 1<<24;
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.WriteNVSpace(
      bad_index, 0, "", &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, WriteNVSpaceBadSessions) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(SAPI_RC_INVALID_SESSIONS, utility.WriteNVSpace(53, 0, "", NULL));
}

TEST_F(TpmUtilityTest, WriteNVSpaceFailure) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t offset = 5;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  EXPECT_CALL(mock_tpm_,
              NV_WriteSync(TPM_RH_OWNER, _, nvram_index, _, _, offset, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.WriteNVSpace(
      index, offset, "", &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, ReadNVSpaceSuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t offset = 5;
  uint32_t nv_index = NV_INDEX_FIRST + index;
  size_t length = 24;
  std::string nvram_data;
  EXPECT_CALL(mock_tpm_,
              NV_ReadSync(nv_index, _, nv_index, _, length, offset, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ReadNVSpace(
      index, offset, length, &nvram_data, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, ReadNVSpaceBadReadLength) {
  TpmUtilityImpl utility(factory_);
  size_t length = 1025;
  std::string nvram_data;
  EXPECT_EQ(SAPI_RC_BAD_SIZE, utility.ReadNVSpace(
      52, 0, length, &nvram_data, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, ReadNVSpaceBadIndex) {
  TpmUtilityImpl utility(factory_);
  uint32_t bad_index = 1<<24;
  std::string nvram_data;
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.ReadNVSpace(
      bad_index, 0, 5, &nvram_data, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, ReadNVSpaceBadSession) {
  TpmUtilityImpl utility(factory_);
  std::string nvram_data;
  EXPECT_EQ(SAPI_RC_INVALID_SESSIONS,
            utility.ReadNVSpace(53, 0, 5, &nvram_data, NULL));
}

TEST_F(TpmUtilityTest, ReadNVSpaceFailure) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t offset = 5;
  uint32_t nv_index = NV_INDEX_FIRST + index;
  size_t length = 24;
  std::string nvram_data;
  EXPECT_CALL(mock_tpm_,
              NV_ReadSync(nv_index, _, nv_index, _, length, offset, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ReadNVSpace(
      index, offset, length, &nvram_data, &mock_authorization_session_));
}

TEST_F(TpmUtilityTest, GetNVSpaceNameSuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  std::string name;
  EXPECT_CALL(mock_tpm_, NV_ReadPublicSync(nvram_index, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.GetNVSpaceName(index, &name));
}

TEST_F(TpmUtilityTest, GetNVSpaceNameFailure) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  std::string name;
  EXPECT_CALL(mock_tpm_, NV_ReadPublicSync(_, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.GetNVSpaceName(index, &name));
}

TEST_F(TpmUtilityTest, GetNVSpacePublicAreaSuccess) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  uint32_t nvram_index = NV_INDEX_FIRST + index;
  TPMS_NV_PUBLIC public_area;
  EXPECT_CALL(mock_tpm_, NV_ReadPublicSync(nvram_index, _, _, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.GetNVSpacePublicArea(index, &public_area));
}

TEST_F(TpmUtilityTest, GetNVSpacePublicAreaFailure) {
  TpmUtilityImpl utility(factory_);
  uint32_t index = 53;
  TPMS_NV_PUBLIC public_area;
  EXPECT_CALL(mock_tpm_, NV_ReadPublicSync(_, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.GetNVSpacePublicArea(index, &public_area));
}

TEST_F(TpmUtilityTest, RootKeysSuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.CreateStorageRootKeys("password"));
}

TEST_F(TpmUtilityTest, RootKeysHandleConsistency) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE test_handle = 42;
  EXPECT_CALL(mock_tpm_, CreatePrimarySyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<3>(test_handle),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, EvictControlSync(_, _, test_handle, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.CreateStorageRootKeys("password"));
}

TEST_F(TpmUtilityTest, RootKeysCreateFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, CreatePrimarySyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.CreateStorageRootKeys("password"));
}

TEST_F(TpmUtilityTest, RootKeysPersistFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, EvictControlSync(_, _, _, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.CreateStorageRootKeys("password"));
}

TEST_F(TpmUtilityTest, SaltingKeySuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.CreateSaltingKey("password"));
}

TEST_F(TpmUtilityTest, SaltingKeyConsistency) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE test_handle = 42;
  EXPECT_CALL(mock_tpm_, LoadSync(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<4>(test_handle),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, EvictControlSync(_, _, test_handle, _, _, _))
      .WillRepeatedly(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.CreateSaltingKey("password"));
}

TEST_F(TpmUtilityTest, SaltingKeyCreateFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, CreateSyncShort(_, _, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.CreateSaltingKey("password"));
}

TEST_F(TpmUtilityTest, SaltingKeyLoadFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, LoadSync(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.CreateSaltingKey("password"));
}

TEST_F(TpmUtilityTest, SaltingKeyPersistFailure) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_, EvictControlSync(_, _, _, _, _, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.CreateSaltingKey("password"));
}

}  // namespace trunks
