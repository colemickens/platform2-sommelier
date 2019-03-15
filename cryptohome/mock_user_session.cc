// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_user_session.h"

using testing::_;
using testing::Invoke;

namespace cryptohome {

MockUserSession::MockUserSession() {
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
  ON_CALL(*this, set_key_index(_))
      .WillByDefault(Invoke(&user_session_, &UserSession::set_key_index));
}

MockUserSession::~MockUserSession() {}

}  // namespace cryptohome
