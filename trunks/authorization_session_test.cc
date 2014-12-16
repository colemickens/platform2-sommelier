// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/authorization_session_impl.h"

#include <base/logging.h>
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

 protected:
  TrunksFactoryForTest factory_;
  NiceMock<MockTpm> mock_tpm_;
};

TEST_F(AuthorizationSessionTest, StartUnboundSuccess) {
  AuthorizationSessionImpl session(factory_);
  TPM2B_PUBLIC public_data;
  public_data.public_area.unique.rsa.size = 256;
  public_data.public_area.unique.rsa.buffer[0] = 1;
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
  public_data.public_area.unique.rsa.size = 256;
  public_data.public_area.unique.rsa.buffer[0] = 1;
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
  public_data.public_area.unique.rsa.size = 256;
  public_data.public_area.unique.rsa.buffer[0] = 1;
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
  public_data.public_area.unique.rsa.size = 256;
  public_data.public_area.unique.rsa.buffer[0] = 1;
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
