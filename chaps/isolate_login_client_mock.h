// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_ISOLATE_LOGIN_CLIENT_MOCK_H_
#define CHAPS_ISOLATE_LOGIN_CLIENT_MOCK_H_

#include "chaps/isolate_login_client.h"

#include <string>
#include <vector>

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace chaps {

class IsolateLoginClientMock : public IsolateLoginClient {
 public:
  IsolateLoginClientMock() : IsolateLoginClient(NULL, NULL, NULL) {}

  MOCK_METHOD2(LoginUser, bool(const std::string&, const brillo::SecureBlob&));
  MOCK_METHOD1(LogoutUser, bool(const std::string&));
  MOCK_METHOD3(ChangeUserAuth,
               bool(const std::string&,
                    const brillo::SecureBlob&,
                    const brillo::SecureBlob&));
};

}  // namespace chaps

#endif  // CHAPS_ISOLATE_LOGIN_CLIENT_MOCK_H_
