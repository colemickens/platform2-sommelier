// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_USER_SESSION_H_
#define CRYPTOHOME_MOCK_USER_SESSION_H_

#include "user_session.h"

#include <base/logging.h>
#include <chromeos/utility.h>

#include <gmock/gmock.h>

namespace cryptohome {
using ::testing::_;
using ::testing::Invoke;

class Crypto;

class MockUserSession : public UserSession {
 public:
  MockUserSession()
      : user_session_() {
    ON_CALL(*this, Init(_))
        .WillByDefault(Invoke(&user_session_, &UserSession::Init));
    ON_CALL(*this, SetUser(_))
        .WillByDefault(Invoke(&user_session_, &UserSession::SetUser));
    ON_CALL(*this, Reset())
        .WillByDefault(Invoke(&user_session_, &UserSession::Reset));
    ON_CALL(*this, CheckUser(_))
        .WillByDefault(Invoke(&user_session_, &UserSession::CheckUser));
    ON_CALL(*this, Verify(_))
        .WillByDefault(Invoke(&user_session_, &UserSession::Verify));
  }
  ~MockUserSession() {}
  MOCK_METHOD1(Init, void(Crypto*));
  MOCK_METHOD1(SetUser, bool(const Credentials&));
  MOCK_METHOD0(Reset, void(void));
  MOCK_CONST_METHOD1(CheckUser, bool(const Credentials&));
  MOCK_CONST_METHOD1(Verify, bool(const Credentials&));
 private:
  UserSession user_session_;
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_USER_SESSION_H_
