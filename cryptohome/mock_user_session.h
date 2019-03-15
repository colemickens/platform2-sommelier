// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_USER_SESSION_H_
#define CRYPTOHOME_MOCK_USER_SESSION_H_

#include "cryptohome/user_session.h"

#include <base/logging.h>
#include <brillo/secure_blob.h>

#include <gmock/gmock.h>

namespace cryptohome {

class MockUserSession : public UserSession {
 public:
  MockUserSession();
  ~MockUserSession();
  MOCK_METHOD1(Init, void(const brillo::SecureBlob&));
  MOCK_METHOD1(SetUser, bool(const Credentials&));
  MOCK_METHOD0(Reset, void(void));
  MOCK_CONST_METHOD1(CheckUser, bool(const Credentials&));
  MOCK_CONST_METHOD1(Verify, bool(const Credentials&));
  MOCK_METHOD1(set_key_index, void(int));
 private:
  UserSession user_session_;
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_USER_SESSION_H_
