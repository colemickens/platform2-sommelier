// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/policy_session_impl.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "trunks/mock_session_manager.h"
#include "trunks/mock_tpm.h"
#include "trunks/trunks_factory_for_test.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace trunks {

class PolicySessionTest : public testing::Test {
 public:
  PolicySessionTest() {}
  ~PolicySessionTest() override {}

  void SetUp() override {
    factory_.set_session_manager(&mock_session_manager_);
    factory_.set_tpm(&mock_tpm_);
  }

  HmacAuthorizationDelegate* GetHmacDelegate(PolicySessionImpl* session) {
    return &(session->hmac_delegate_);
  }

 protected:
  TrunksFactoryForTest factory_;
  NiceMock<MockSessionManager> mock_session_manager_;
  NiceMock<MockTpm> mock_tpm_;
};

TEST_F(PolicySessionTest, GetDelegateUninitialized) {
  PolicySessionImpl session(factory_);
  EXPECT_CALL(mock_session_manager_, GetSessionHandle())
      .WillRepeatedly(Return(kUninitializedHandle));
  EXPECT_EQ(NULL, session.GetDelegate());
}

TEST_F(PolicySessionTest, GetDelegateSuccess) {
  PolicySessionImpl session(factory_);
  EXPECT_CALL(mock_session_manager_, GetSessionHandle())
      .WillRepeatedly(Return(TPM_RH_FIRST));
  EXPECT_EQ(GetHmacDelegate(&session), session.GetDelegate());
}

TEST_F(PolicySessionTest, StartBoundSessionSuccess) {
  PolicySessionImpl session(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS,
            session.StartBoundSession(TPM_RH_FIRST, "auth", true));
}

TEST_F(PolicySessionTest, StartBoundSessionFailure) {
  PolicySessionImpl session(factory_);
  TPM_HANDLE handle = TPM_RH_FIRST;
  EXPECT_CALL(mock_session_manager_, StartSession(TPM_SE_POLICY, handle,
                                                  _, true, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, session.StartBoundSession(handle, "auth", true));
}

TEST_F(PolicySessionTest, StartUnboundSessionSuccess) {
  PolicySessionImpl session(factory_);
  EXPECT_EQ(TPM_RC_SUCCESS, session.StartUnboundSession(true));
}

TEST_F(PolicySessionTest, StartUnboundSessionFailure) {
  PolicySessionImpl session(factory_);
  EXPECT_CALL(mock_session_manager_, StartSession(TPM_SE_POLICY, TPM_RH_NULL,
                                                  _, true, _))
      .WillRepeatedly(Return(TPM_RC_FAILURE));
  EXPECT_EQ(TPM_RC_FAILURE, session.StartUnboundSession(true));
}

}  // namespace trunks
