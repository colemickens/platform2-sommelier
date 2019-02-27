// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A mock token manager

#ifndef CHAPS_TOKEN_FILE_MANAGER_MOCK_H_
#define CHAPS_TOKEN_FILE_MANAGER_MOCK_H_

#include "chaps/token_file_manager.h"

#include <string>

#include <base/files/file_path.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace chaps {

class TokenFileManagerMock : public TokenFileManager {
 public:
  TokenFileManagerMock() : TokenFileManager(-1, -1) {}

  MOCK_METHOD2(GetUserTokenPath, bool(const std::string&, base::FilePath*));
  MOCK_METHOD1(CreateUserTokenDirectory, bool(const base::FilePath&));
  MOCK_METHOD1(CheckUserTokenPermissions, bool(const base::FilePath&));
  MOCK_METHOD3(SaltAuthData,
               bool(const base::FilePath&,
                    const brillo::SecureBlob&,
                    brillo::SecureBlob*));
};

}  // namespace chaps

#endif  // CHAPS_TOKEN_FILE_MANAGER_MOCK_H_
