// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "trunks/error_codes.h"
#include "trunks/mock_authorization_delegate.h"
#include "trunks/mock_authorization_session.h"
#include "trunks/mock_tpm.h"
#include "trunks/mock_tpm_state.h"
#include "trunks/null_authorization_delegate.h"
#include "trunks/tpm_utility_impl.h"
#include "trunks/trunks_factory_for_test.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;

namespace trunks {

namespace {

const trunks::TPMA_OBJECT kRestricted = 1U << 16;
const trunks::TPMA_OBJECT kDecrypt = 1U << 17;
const trunks::TPMA_OBJECT kSign = 1U << 18;

}  // namespace

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

TEST_F(TpmUtilityTest, StirRandomSuccess) {
  TpmUtilityImpl utility(factory_);
  std::string entropy_data("large test data", 100);
  EXPECT_CALL(mock_tpm_, StirRandomSync(_, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.StirRandom(entropy_data));
}

TEST_F(TpmUtilityTest, StirRandomFails) {
  TpmUtilityImpl utility(factory_);
  std::string entropy_data("test data");
  EXPECT_CALL(mock_tpm_, StirRandomSync(_, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.StirRandom(entropy_data));
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

  EXPECT_CALL(mock_tpm_, GetRandomSync(_, _, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<1>(large_random),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, GetRandomSync(8, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(small_random),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.GenerateRandom(num_bytes, &random_data));
  EXPECT_EQ(num_bytes, random_data.size());
}

TEST_F(TpmUtilityTest, GenerateRandomFails) {
  TpmUtilityImpl utility(factory_);
  int num_bytes = 5;
  std::string random_data;
  EXPECT_CALL(mock_tpm_, GetRandomSync(_, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.GenerateRandom(num_bytes, &random_data));
}

TEST_F(TpmUtilityTest, ExtendPCRSuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, utility.ExtendPCR(1, "test digest"));
}

TEST_F(TpmUtilityTest, ExtendPCRFail) {
  TpmUtilityImpl utility(factory_);
  int pcr_index = 0;
  TPM_HANDLE pcr_handle = HR_PCR + pcr_index;
  EXPECT_CALL(mock_tpm_, PCR_ExtendSync(pcr_handle, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.ExtendPCR(pcr_index, "test digest"));
}

TEST_F(TpmUtilityTest, ExtendPCRBadParam) {
  TpmUtilityImpl utility(factory_);
  EXPECT_EQ(TPM_RC_FAILURE, utility.ExtendPCR(-1, "test digest"));
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

TEST_F(TpmUtilityTest, TakeOwnershipSuccess) {
  TpmUtilityImpl utility(factory_);
  EXPECT_CALL(mock_tpm_state_, IsOwnerPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsEndorsementPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_state_, IsLockoutPasswordSet())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_, HierarchyChangeAuthSync(TPM_RH_OWNER, _, _, _))
      .Times(1);
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
      .Times(0);
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
      .Times(0);
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
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricEncrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      plaintext,
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
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.AsymmetricEncrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      plaintext,
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
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.AsymmetricEncrypt(key_handle,
                                                             TPM_ALG_RSAES,
                                                             TPM_ALG_NULL,
                                                             plaintext,
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
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricEncrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      plaintext,
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
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_EncryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricEncrypt(key_handle,
                                                      TPM_ALG_RSAES,
                                                      TPM_ALG_NULL,
                                                      plaintext,
                                                      &ciphertext));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSAES);
}

TEST_F(TpmUtilityTest, AsymmetricDecryptSuccess) {
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
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricDecrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      password,
                                                      ciphertext,
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
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.AsymmetricDecrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      password,
                                                      ciphertext,
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
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.AsymmetricDecrypt(key_handle,
                                                             TPM_ALG_RSAES,
                                                             TPM_ALG_NULL,
                                                             password,
                                                             ciphertext,
                                                             &plaintext));
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
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricDecrypt(key_handle,
                                                      TPM_ALG_NULL,
                                                      TPM_ALG_NULL,
                                                      password,
                                                      ciphertext,
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
  TPMT_RSA_DECRYPT scheme;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, RSA_DecryptSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(out_message),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.AsymmetricDecrypt(key_handle,
                                                      TPM_ALG_RSAES,
                                                      TPM_ALG_NULL,
                                                      password,
                                                      ciphertext,
                                                      &plaintext));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSAES);
}

TEST_F(TpmUtilityTest, SignSuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  TPMT_SIGNATURE signature_out;
  signature_out.signature.rsassa.sig.size = 2;
  signature_out.signature.rsassa.sig.buffer[0] = 'h';
  signature_out.signature.rsassa.sig.buffer[1] = 'i';
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, SignSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(signature_out),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Sign(key_handle,
                                         TPM_ALG_NULL,
                                         TPM_ALG_NULL,
                                         password,
                                         digest,
                                         &signature));
  EXPECT_EQ(0, signature.compare("hi"));
}

TEST_F(TpmUtilityTest, SignFail) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, SignSync(key_handle, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, utility.Sign(key_handle,
                                         TPM_ALG_NULL,
                                         TPM_ALG_NULL,
                                         password,
                                         digest,
                                         &signature));
}

TEST_F(TpmUtilityTest, SignBadParams1) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign | kRestricted;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_RSAPSS,
                                                TPM_ALG_NULL,
                                                password,
                                                digest,
                                                &signature));
}

TEST_F(TpmUtilityTest, SignBadParams2) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kDecrypt;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_RSAPSS,
                                                TPM_ALG_NULL,
                                                password,
                                                digest,
                                                &signature));
}

TEST_F(TpmUtilityTest, SignBadParams3) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_ECC;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_RSAPSS,
                                                TPM_ALG_NULL,
                                                password,
                                                digest,
                                                &signature));
}

TEST_F(TpmUtilityTest, SignBadParams4) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_FAILURE)));
  EXPECT_EQ(TPM_RC_FAILURE, utility.Sign(key_handle,
                                         TPM_ALG_RSAPSS,
                                         TPM_ALG_NULL,
                                         password,
                                         digest,
                                         &signature));
}

TEST_F(TpmUtilityTest, SignBadParams5) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle = 0;
  std::string password;
  std::string digest;
  std::string signature;
  NullAuthorizationDelegate delegate;
  EXPECT_EQ(SAPI_RC_BAD_PARAMETER, utility.Sign(key_handle,
                                                TPM_ALG_AES,
                                                TPM_ALG_NULL,
                                                password,
                                                digest,
                                                &signature));
}


TEST_F(TpmUtilityTest, SignNullSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  TPMT_SIGNATURE signature_out;
  signature_out.signature.rsassa.sig.size = 0;
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  TPMT_SIG_SCHEME scheme;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
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
                                         password,
                                         digest,
                                         &signature));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSASSA);
  EXPECT_EQ(scheme.details.rsassa.hash_alg, TPM_ALG_SHA256);
}

TEST_F(TpmUtilityTest, SignSchemeForward) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string password;
  std::string digest;
  TPMT_SIGNATURE signature_out;
  signature_out.signature.rsassa.sig.size = 0;
  std::string signature;
  NullAuthorizationDelegate delegate;
  TPM2B_PUBLIC public_area;
  TPMT_SIG_SCHEME scheme;
  public_area.public_area.type = TPM_ALG_RSA;
  public_area.public_area.object_attributes = kSign;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(key_handle, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(public_area),
                            Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, SignSync(key_handle, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(signature_out),
                      SaveArg<3>(&scheme),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, utility.Sign(key_handle,
                                         TPM_ALG_RSAPSS,
                                         TPM_ALG_SHA512,
                                         password,
                                         digest,
                                         &signature));
  EXPECT_EQ(scheme.scheme, TPM_ALG_RSAPSS);
  EXPECT_EQ(scheme.details.rsapss.hash_alg, TPM_ALG_SHA512);
}

TEST_F(TpmUtilityTest, VerifySuccess) {
  TpmUtilityImpl utility(factory_);
  TPM_HANDLE key_handle;
  std::string digest;
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
  std::string digest;
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
  std::string digest;
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
  std::string digest;
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
  std::string digest;
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
  std::string digest;
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
  std::string digest;
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
  std::string digest;
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
  std::string digest;
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
                                           TPM_ALG_SHA512,
                                           digest,
                                           signature));
  EXPECT_EQ(signature_in.sig_alg, TPM_ALG_RSAPSS);
  EXPECT_EQ(signature_in.signature.rsassa.hash, TPM_ALG_SHA512);
}

}  // namespace trunks
