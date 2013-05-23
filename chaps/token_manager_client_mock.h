// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TOKEN_MANAGER_CLIENT_MOCK_H_
#define CHAPS_TOKEN_MANAGER_CLIENT_MOCK_H_

#include "chaps/token_manager_client.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace chaps {

class TokenManagerClientMock : public TokenManagerClient {
 public:
  MOCK_METHOD2(OpenIsolate, bool (chromeos::SecureBlob*, bool*));
  MOCK_METHOD1(CloseIsolate, void (const chromeos::SecureBlob&));
  MOCK_METHOD5(LoadToken, bool (const chromeos::SecureBlob&,
                                const FilePath&,
                                const chromeos::SecureBlob&,
                                const std::string&,
                                int*));
  MOCK_METHOD2(UnloadToken, void (const chromeos::SecureBlob&,
                                  const FilePath&));
  MOCK_METHOD3(ChangeTokenAuthData, void (const FilePath&,
                                          const chromeos::SecureBlob&,
                                          const chromeos::SecureBlob&));
  MOCK_METHOD3(GetTokenPath, bool(const chromeos::SecureBlob&,
                                  int,
                                  FilePath*));
};

}  // namespace chaps

#endif  // CHAPS_TOKEN_MANAGER_CLIENT_MOCK_H_
