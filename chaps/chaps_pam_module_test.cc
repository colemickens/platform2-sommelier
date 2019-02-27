// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chaps pam module unit tests. These tests exercise the pam module layer and
// use a mock for the login manager and pam helper.
//

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <string>
#include <vector>

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/chaps_utility.h"
#include "chaps/isolate_login_client_mock.h"
#include "chaps/pam_helper_mock.h"

using brillo::SecureBlob;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace chaps {

// Defined in chaps_pam_module.cc.
extern void EnableMock(IsolateLoginClient* login_client, PamHelper* pam_helper);
extern void DisableMock();

class TestPamModule : public ::testing::Test {
 protected:
  void SetUp() override {
    user_ = string("user");
    password_old_ = SecureBlob("password_old");
    password_new_ = SecureBlob("password_new");

    EnableMock(&login_client_mock_, &pam_helper_mock_);

    EXPECT_CALL(pam_helper_mock_, GetPamUser(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(user_), Return(true)));
    EXPECT_CALL(pam_helper_mock_, GetPamPassword(_, false, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(password_new_), Return(true)));
    EXPECT_CALL(pam_helper_mock_, GetPamPassword(_, true, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(password_old_), Return(true)));
    EXPECT_CALL(pam_helper_mock_, SaveUserAndPassword(_, user_, password_new_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(pam_helper_mock_, RetrieveUserAndPassword(_, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(user_),
                              SetArgPointee<2>(password_new_), Return(true)));
    EXPECT_CALL(pam_helper_mock_, PutEnvironmentVariable(_, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(pam_helper_mock_, GetEnvironmentVariable(_, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(string("1")), Return(true)));
  }
  void TearDown() { DisableMock(); }
  string user_;
  SecureBlob password_old_;
  SecureBlob password_new_;
  IsolateLoginClientMock login_client_mock_;
  PamHelperMock pam_helper_mock_;
};

TEST_F(TestPamModule, TestPamAuthenticateSuccess) {
  EXPECT_CALL(login_client_mock_, LoginUser(user_, password_new_))
      .WillOnce(Return(true));

  EXPECT_EQ(PAM_SUCCESS, pam_sm_authenticate(NULL, 0, 0, NULL));
  EXPECT_EQ(PAM_SUCCESS, pam_sm_open_session(NULL, 0, 0, NULL));
}

TEST_F(TestPamModule, TestPamOpenWithoutAuthenticate) {
  EXPECT_CALL(pam_helper_mock_, RetrieveUserAndPassword(_, _, _))
      .WillRepeatedly(Return(false));

  EXPECT_EQ(PAM_IGNORE, pam_sm_open_session(NULL, 0, 0, NULL));
}

TEST_F(TestPamModule, TestPamOpenWithoutDifferentUser) {
  EXPECT_CALL(pam_helper_mock_, GetPamUser(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(user_), Return(true)))
      .WillRepeatedly(DoAll(SetArgPointee<1>(string("user_2")), Return(true)));

  EXPECT_EQ(PAM_SUCCESS, pam_sm_authenticate(NULL, 0, 0, NULL));
  EXPECT_EQ(PAM_IGNORE, pam_sm_open_session(NULL, 0, 0, NULL));
}

TEST_F(TestPamModule, TestPamCloseSuccess) {
  EXPECT_CALL(login_client_mock_, LogoutUser(user_)).WillOnce(Return(true));

  EXPECT_EQ(PAM_SUCCESS, pam_sm_close_session(NULL, 0, 0, NULL));
}

TEST_F(TestPamModule, TestPamChangeAuthSuccess) {
  EXPECT_CALL(login_client_mock_,
              ChangeUserAuth(user_, password_old_, password_new_))
      .WillOnce(Return(true));

  EXPECT_EQ(PAM_SUCCESS, pam_sm_chauthtok(NULL, PAM_UPDATE_AUTHTOK, 0, NULL));
}

TEST_F(TestPamModule, TestPamChangeAuthPrelimCheck) {
  EXPECT_EQ(
      PAM_IGNORE,
      pam_sm_chauthtok(NULL, PAM_PRELIM_CHECK | PAM_UPDATE_AUTHTOK, 0, NULL));
  EXPECT_EQ(PAM_IGNORE, pam_sm_chauthtok(NULL, 0, 0, NULL));
}

TEST_F(TestPamModule, TestPamChangeAuthFail) {
  EXPECT_CALL(pam_helper_mock_, GetPamPassword(_, true, _))
      .WillOnce(Return(false))
      .WillRepeatedly(DoAll(SetArgPointee<2>(password_old_), Return(true)));
  EXPECT_CALL(pam_helper_mock_, GetPamPassword(_, false, _))
      .WillOnce(Return(false));

  EXPECT_EQ(PAM_AUTH_ERR, pam_sm_chauthtok(NULL, PAM_UPDATE_AUTHTOK, 0, NULL));
  EXPECT_EQ(PAM_AUTH_ERR, pam_sm_chauthtok(NULL, PAM_UPDATE_AUTHTOK, 0, NULL));
}

}  // namespace chaps
