// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UserSession.

#include "user_session.h"

#include <chromeos/utility.h>
#include <gtest/gtest.h>
#include <string>

#include "crypto.h"
#include "username_passkey.h"

namespace cryptohome {

class UserSessionTest : public ::testing::Test {
 public:
  UserSessionTest()
      : salt() { }
  virtual ~UserSessionTest() { }

  void SetUp() {
    Crypto crypto;
    salt.resize(16);
    crypto.GetSecureRandom(static_cast<unsigned char*>(salt.data()),
                           salt.size());
  }

 protected:
  SecureBlob salt;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserSessionTest);
};

TEST_F(UserSessionTest, InitTest) {
  UsernamePasskey up("username", SecureBlob("password", 8));
  Crypto crypto;
  UserSession session;
  session.Init(&crypto, salt);
  EXPECT_TRUE(session.SetUser(up));
}

TEST_F(UserSessionTest, CheckUserTest) {
  UsernamePasskey up("username", SecureBlob("password", 8));
  Crypto crypto;
  UserSession session;
  session.Init(&crypto, salt);
  EXPECT_TRUE(session.SetUser(up));
  EXPECT_TRUE(session.CheckUser(up));
}

TEST_F(UserSessionTest, ReInitTest) {
  UsernamePasskey up("username", SecureBlob("password", 8));
  UsernamePasskey up_new("username2", SecureBlob("password2", 9));
  Crypto crypto;
  UserSession session;
  session.Init(&crypto, salt);
  EXPECT_TRUE(session.SetUser(up));
  EXPECT_TRUE(session.SetUser(up_new));
  EXPECT_FALSE(session.CheckUser(up));
  EXPECT_TRUE(session.CheckUser(up_new));
}

TEST_F(UserSessionTest, ResetTest) {
  UsernamePasskey up("username", SecureBlob("password", 8));
  Crypto crypto;
  UserSession session;
  session.Init(&crypto, salt);
  EXPECT_TRUE(session.SetUser(up));
  session.Reset();
  EXPECT_FALSE(session.CheckUser(up));
}

TEST_F(UserSessionTest, VerifyTest) {
  UsernamePasskey up("username", SecureBlob("password", 8));
  Crypto crypto;
  UserSession session;
  session.Init(&crypto, salt);
  EXPECT_TRUE(session.SetUser(up));
  EXPECT_TRUE(session.Verify(up));
}

}  // namespace cryptohome
