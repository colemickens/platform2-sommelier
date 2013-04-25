// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_LOGIN_EVENT_CLIENT_MOCK_H_
#define CHAPS_LOGIN_EVENT_CLIENT_MOCK_H_

#include "chaps/login_event_client.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace chaps {

class LoginEventClientMock : public LoginEventClient {
 public:
  MOCK_METHOD2(OpenIsolate, bool (chromeos::SecureBlob*, bool*));
  MOCK_METHOD1(CloseIsolate, void (const chromeos::SecureBlob&));
  MOCK_METHOD4(LoadToken, bool (const chromeos::SecureBlob&,
                                const std::string&,
                                const chromeos::SecureBlob&,
                                int*));
  MOCK_METHOD2(UnloadToken, void (const chromeos::SecureBlob&,
                                  const std::string&));
  MOCK_METHOD3(ChangeTokenAuthData, void (const std::string&,
                                          const chromeos::SecureBlob&,
                                          const chromeos::SecureBlob&));
};

}  // namespace chaps

#endif  // CHAPS_LOGIN_EVENT_CLIENT_MOCK_H_
