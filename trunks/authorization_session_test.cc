// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/authorization_session_impl.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "trunks/mock_tpm.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_utility.h"
#include "trunks/trunks_factory_for_test.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;

namespace trunks {

class AuthorizationSessionTest : public testing::Test {
 public:
  AuthorizationSessionTest() {}
  virtual ~AuthorizationSessionTest() {}

  void SetUp() {
    factory_.set_tpm(&mock_tpm_);
  }

  HmacAuthorizationDelegate* GetHmacDelegate(
        AuthorizationSessionImpl* session) {
    return &(session->hmac_delegate_);
  }

  TPM2B_PUBLIC_KEY_RSA GetValidRSAPublicKey() {
    const char kValidModulus[] =
        "A1D50D088994000492B5F3ED8A9C5FC8772706219F4C063B2F6A8C6B74D3AD6B"
        "212A53D01DABB34A6261288540D420D3BA59ED279D859DE6227A7AB6BD88FADD"
        "FC3078D465F4DF97E03A52A587BD0165AE3B180FE7B255B7BEDC1BE81CB1383F"
        "E9E46F9312B1EF28F4025E7D332E33F4416525FEB8F0FC7B815E8FBB79CDABE6"
        "327B5A155FEF13F559A7086CB8A543D72AD6ECAEE2E704FF28824149D7F4E393"
        "D3C74E721ACA97F7ADBE2CCF7B4BCC165F7380F48065F2C8370F25F066091259"
        "D14EA362BAF236E3CD8771A94BDEDA3900577143A238AB92B6C55F11DEFAFB31"
        "7D1DC5B6AE210C52B008D87F2A7BFF6EB5C4FB32D6ECEC6505796173951A3167";
    std::vector<uint8> bytes;
    CHECK(base::HexStringToBytes(kValidModulus, &bytes));
    CHECK_EQ(bytes.size(), 256u);
    TPM2B_PUBLIC_KEY_RSA rsa;
    rsa.size = bytes.size();
    memcpy(rsa.buffer, bytes.data(), bytes.size());
    return rsa;
  }

 protected:
  TrunksFactoryForTest factory_;
  NiceMock<MockTpm> mock_tpm_;
};

TEST_F(AuthorizationSessionTest, StartUnboundSuccess) {
  AuthorizationSessionImpl session(factory_);
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_, ReadPublicSync(kSaltingKey, _, _, _, _, NULL))
      .WillOnce(DoAll(SetArgPointee<2>(public_data),
                      Return(TPM_RC_SUCCESS)));
  TPM_HANDLE session_handle = TPM_RH_NULL;
  TPM2B_NONCE nonce;
  nonce.size = 20;
  EXPECT_CALL(mock_tpm_, StartAuthSessionSyncShort(_,
                                                   TPM_RH_NULL,
                                                   _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<7>(session_handle),
                      SetArgPointee<8>(nonce),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, session.StartUnboundSession(false));
  EXPECT_EQ(session_handle, GetHmacDelegate(&session)->session_handle());
  EXPECT_CALL(mock_tpm_, FlushContextSync(session_handle, _, _))
      .WillOnce(Return(TPM_RC_SUCCESS));
}

TEST_F(AuthorizationSessionTest, StartUnboundWithBadSaltingKey) {
  AuthorizationSessionImpl session(factory_);
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa.size = 32;
  EXPECT_CALL(mock_tpm_, ReadPublicSync(kSaltingKey, _, _, _, _, NULL))
      .WillOnce(DoAll(SetArgPointee<2>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_FAILURE, session.StartUnboundSession(false));
}

TEST_F(AuthorizationSessionTest, StartUnboundFail) {
  AuthorizationSessionImpl session(factory_);
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_, ReadPublicSync(kSaltingKey, _, _, _, _, NULL))
      .WillOnce(DoAll(SetArgPointee<2>(public_data),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_CALL(mock_tpm_, StartAuthSessionSyncShort(_,
                                                   TPM_RH_NULL,
                                                   _, _, _, _, _, _, _, _))
      .WillOnce(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, session.StartUnboundSession(false));
}

TEST_F(AuthorizationSessionTest, StartUnboundWithBadNonce) {
  AuthorizationSessionImpl session(factory_);
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_, ReadPublicSync(kSaltingKey, _, _, _, _, NULL))
      .WillOnce(DoAll(SetArgPointee<2>(public_data),
                      Return(TPM_RC_SUCCESS)));
  TPM2B_NONCE nonce;
  nonce.size = 0;
  EXPECT_CALL(mock_tpm_, StartAuthSessionSyncShort(_,
                                                   TPM_RH_NULL,
                                                   _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<8>(nonce),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_FAILURE, session.StartUnboundSession(false));
}

TEST_F(AuthorizationSessionTest, StartBoundSuccess) {
  AuthorizationSessionImpl session(factory_);
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa = GetValidRSAPublicKey();
  EXPECT_CALL(mock_tpm_, ReadPublicSync(kSaltingKey, _, _, _, _, NULL))
      .WillOnce(DoAll(SetArgPointee<2>(public_data),
                      Return(TPM_RC_SUCCESS)));
  TPM_HANDLE session_handle = TPM_RH_NULL;
  TPM_HANDLE bind_handle = TPM_RH_FIRST;
  TPM2B_NONCE nonce;
  nonce.size = 20;
  EXPECT_CALL(mock_tpm_, StartAuthSessionSyncShort(_,
                                                   bind_handle,
                                                   _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<7>(session_handle),
                      SetArgPointee<8>(nonce),
                      Return(TPM_RC_SUCCESS)));
  EXPECT_EQ(TPM_RC_SUCCESS, session.StartBoundSession(bind_handle, "", false));
  EXPECT_EQ(session_handle, GetHmacDelegate(&session)->session_handle());
}

TEST_F(AuthorizationSessionTest, EntityAuthorizationForwardingTest) {
  AuthorizationSessionImpl session(factory_);
  std::string test_auth("test_auth");
  session.SetEntityAuthorizationValue(test_auth);
  HmacAuthorizationDelegate* hmac_delegate = GetHmacDelegate(&session);
  std::string entity_auth = hmac_delegate->entity_auth_value();
  EXPECT_EQ(0, test_auth.compare(entity_auth));
}

TEST_F(AuthorizationSessionTest, FutureAuthorizationForwardingTest) {
    AuthorizationSessionImpl session(factory_);
  std::string test_auth("test_auth");
  session.SetFutureAuthorizationValue(test_auth);
  HmacAuthorizationDelegate* hmac_delegate = GetHmacDelegate(&session);
  std::string entity_auth = hmac_delegate->future_authorization_value();
  EXPECT_EQ(0, test_auth.compare(entity_auth));
}

}  // namespace trunks
