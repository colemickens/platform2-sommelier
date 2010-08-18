// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_NSS_UTIL_H_
#define LOGIN_MANAGER_MOCK_NSS_UTIL_H_

#include "login_manager/nss_util.h"

#include <unistd.h>
#include <base/file_path.h>
#include <gmock/gmock.h>

namespace login_manager {
using ::testing::Return;

class MockNssUtil : public NssUtil {
 public:
  MockNssUtil() {
    EXPECT_CALL(*this, GetOwnerKeyFilePath())
        .WillOnce(Return(FilePath("")));
  }
  virtual ~MockNssUtil() {}

  MOCK_METHOD0(OpenUserDB, bool());
  MOCK_METHOD1(CheckOwnerKey, bool(const std::vector<uint8>&));
  MOCK_METHOD0(GetOwnerKeyFilePath, FilePath());
  MOCK_METHOD8(Verify, bool(const uint8* algorithm, int algorithm_len,
                            const uint8* signature, int signature_len,
                            const uint8* data, int data_len,
                            const uint8* public_key, int public_key_len));
};

template<typename T>
class MockFactory : public NssUtil::Factory {
 public:
  MockFactory() {}
  ~MockFactory() {}
  NssUtil* CreateNssUtil() {
    return new T;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MockFactory);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_NSS_UTIL_H_
