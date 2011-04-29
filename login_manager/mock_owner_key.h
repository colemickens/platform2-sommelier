// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_OWNER_KEY_H_
#define LOGIN_MANAGER_MOCK_OWNER_KEY_H_

#include "login_manager/owner_key.h"

#include <unistd.h>
#include <gmock/gmock.h>

namespace base {
class RSAPrivateKey;
}

namespace login_manager {
class MockOwnerKey : public OwnerKey {
 public:
  MockOwnerKey() : OwnerKey(FilePath("")) {}
  virtual ~MockOwnerKey() {}
  MOCK_CONST_METHOD1(Equals, bool(const std::string&));
  MOCK_CONST_METHOD1(VEquals, bool(const std::vector<uint8>&));
  MOCK_CONST_METHOD0(HaveCheckedDisk, bool());
  MOCK_CONST_METHOD0(IsPopulated, bool());
  MOCK_METHOD0(PopulateFromDiskIfPossible, bool());
  MOCK_METHOD1(PopulateFromBuffer, bool(const std::vector<uint8>&));
  MOCK_METHOD1(PopulateFromKeypair, bool(base::RSAPrivateKey*));
  MOCK_METHOD0(Persist, bool());
  MOCK_METHOD3(Rotate, bool(const std::vector<uint8>&, const uint8*, uint32));
  MOCK_METHOD1(ClobberCompromisedKey, bool(const std::vector<uint8>&));
  MOCK_METHOD4(Verify, bool(const uint8*, uint32, const uint8*, uint32));
  MOCK_METHOD3(Sign, bool(const uint8*, uint32, std::vector<uint8>*));
  MOCK_METHOD1(StartGeneration, int(ChildJobInterface*));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_OWNER_KEY_H_
