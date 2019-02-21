// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Isolate login client unit tests.
//

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/isolate_login_client.h"
#include "chaps/isolate_mock.h"
#include "chaps/token_file_manager_mock.h"
#include "chaps/token_manager_client_mock.h"

using base::FilePath;
using brillo::SecureBlob;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace chaps {

class TestIsolateLoginClient : public ::testing::Test {
 protected:
  void SetUp() override {
    user_ = string("user");
    auth_old_ = SecureBlob("auth_old");
    auth_new_ = SecureBlob("auth_new");
    salted_auth_old_ = SecureBlob("salted_auth_old");
    salted_auth_new_ = SecureBlob("salted_auth_new");
    isolate_credential_ = SecureBlob("credential");
    token_path_ = FilePath("token_path");
    isolate_login_client_.reset(new IsolateLoginClient(&isolate_manager_mock_,
                                                       &file_manager_mock_,
                                                       &token_manager_mock_));

    EXPECT_CALL(isolate_manager_mock_, GetUserIsolateCredential(user_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(isolate_credential_),
                              Return(true)));
    EXPECT_CALL(token_manager_mock_, OpenIsolate(_, _))
         .WillRepeatedly(DoAll(SetArgPointee<1>(false), Return(true)));
    EXPECT_CALL(file_manager_mock_, GetUserTokenPath(user_, _))
         .WillRepeatedly(DoAll(SetArgPointee<1>(token_path_),
                               Return(true)));
    EXPECT_CALL(file_manager_mock_, CheckUserTokenPermissions(token_path_))
         .WillRepeatedly(Return(true));
    EXPECT_CALL(file_manager_mock_, SaltAuthData(token_path_, auth_old_, _))
         .WillRepeatedly(DoAll(SetArgPointee<2>(salted_auth_old_),
                               Return(true)));
    EXPECT_CALL(file_manager_mock_, SaltAuthData(token_path_, auth_new_, _))
         .WillRepeatedly(DoAll(SetArgPointee<2>(salted_auth_new_),
                               Return(true)));
  }
  string user_;
  SecureBlob auth_old_;
  SecureBlob auth_new_;
  SecureBlob salted_auth_old_;
  SecureBlob salted_auth_new_;
  SecureBlob isolate_credential_;
  FilePath token_path_;
  IsolateCredentialManagerMock isolate_manager_mock_;
  TokenFileManagerMock file_manager_mock_;
  TokenManagerClientMock token_manager_mock_;
  std::unique_ptr<IsolateLoginClient> isolate_login_client_;
};

TEST_F(TestIsolateLoginClient, TestLoginUserSuccess) {
  EXPECT_CALL(token_manager_mock_, LoadToken(isolate_credential_,
                                             token_path_,
                                             salted_auth_new_,
                                             user_,
                                             _))
      .WillOnce(Return(true));

  EXPECT_TRUE(isolate_login_client_->LoginUser(user_, auth_new_));
}

TEST_F(TestIsolateLoginClient, TestLoginUserFail) {
  EXPECT_CALL(token_manager_mock_, OpenIsolate(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(false), Return(false)))
      .WillRepeatedly(DoAll(SetArgPointee<1>(false), Return(true)));
  EXPECT_CALL(token_manager_mock_, LoadToken(isolate_credential_,
                                            token_path_,
                                            salted_auth_new_,
                                            user_,
                                            _))
      .WillOnce(Return(false));

  EXPECT_FALSE(isolate_login_client_->LoginUser(user_, auth_new_));
  EXPECT_FALSE(isolate_login_client_->LoginUser(user_, auth_new_));
}

TEST_F(TestIsolateLoginClient, TestLoginUserNewIsolate) {
  EXPECT_CALL(isolate_manager_mock_, GetUserIsolateCredential(user_, _))
      .WillOnce(DoAll(SetArgPointee<1>(SecureBlob("invalid")),
                      Return(true)));
  EXPECT_CALL(token_manager_mock_, OpenIsolate(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(isolate_credential_),
                      SetArgPointee<1>(true),
                      Return(true)));
  EXPECT_CALL(isolate_manager_mock_,
              SaveIsolateCredential(user_, isolate_credential_))
      .WillOnce(Return(true));
  EXPECT_CALL(token_manager_mock_, LoadToken(isolate_credential_,
                                             token_path_,
                                             salted_auth_new_,
                                             user_,
                                             _))
      .WillOnce(Return(true));

  EXPECT_TRUE(isolate_login_client_->LoginUser(user_, auth_new_));
}

TEST_F(TestIsolateLoginClient, TestLoginUserCreateToken) {
  EXPECT_CALL(file_manager_mock_, GetUserTokenPath(user_, _))
       .WillOnce(DoAll(SetArgPointee<1>(token_path_), Return(false)));
  EXPECT_CALL(file_manager_mock_, CreateUserTokenDirectory(token_path_))
       .WillOnce(Return(true));
  EXPECT_CALL(token_manager_mock_, LoadToken(isolate_credential_,
                                             token_path_,
                                             salted_auth_new_,
                                             user_,
                                             _))
      .WillOnce(Return(true));

  EXPECT_TRUE(isolate_login_client_->LoginUser(user_, auth_new_));
}

TEST_F(TestIsolateLoginClient, TestLoginUserBadTokenPerms) {
  EXPECT_CALL(file_manager_mock_, CheckUserTokenPermissions(token_path_))
       .WillOnce(Return(false));

  EXPECT_FALSE(isolate_login_client_->LoginUser(user_, auth_new_));
}

TEST_F(TestIsolateLoginClient, TestLogoutSuccess) {
  EXPECT_CALL(token_manager_mock_, CloseIsolate(isolate_credential_));

  EXPECT_TRUE(isolate_login_client_->LogoutUser(user_));
}

TEST_F(TestIsolateLoginClient, TestLogoutInvalid) {
  EXPECT_CALL(isolate_manager_mock_, GetUserIsolateCredential(user_, _))
      .WillOnce(Return(false));

  EXPECT_FALSE(isolate_login_client_->LogoutUser(user_));
}

TEST_F(TestIsolateLoginClient, TestChangeUsersAuthSuccess) {
  EXPECT_CALL(token_manager_mock_, ChangeTokenAuthData(token_path_,
                                                       salted_auth_old_,
                                                       salted_auth_new_));

  EXPECT_TRUE(isolate_login_client_->ChangeUserAuth(user_, auth_old_,
                                                    auth_new_));
}

TEST_F(TestIsolateLoginClient, TestChangeUsersAuthNoToken) {
  EXPECT_CALL(file_manager_mock_, GetUserTokenPath(user_, _))
       .WillOnce(DoAll(SetArgPointee<1>(token_path_), Return(false)));

  EXPECT_FALSE(isolate_login_client_->ChangeUserAuth(user_, auth_old_,
                                                     auth_new_));
}

TEST_F(TestIsolateLoginClient, TestChangeUsersAuthBadTokenPerms) {
  EXPECT_CALL(file_manager_mock_, CheckUserTokenPermissions(token_path_))
       .WillOnce(Return(false));

  EXPECT_FALSE(isolate_login_client_->ChangeUserAuth(user_, auth_old_,
                                                     auth_new_));
}

}  // namespace chaps
