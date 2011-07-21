// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_PLATFORM_H_
#define CRYPTOHOME_MOCK_PLATFORM_H_

#include "mount.h"

#include <gmock/gmock.h>

namespace cryptohome {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class MockPlatform : public Platform {
 public:
  MockPlatform() {
    ON_CALL(*this, SetOwnership(_, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, GetUserId(_, _, _))
        .WillByDefault(Invoke(this, &MockPlatform::MockGetUserId));
    ON_CALL(*this, TerminatePidsWithOpenFiles(_, _))
        .WillByDefault(Return(false));
    ON_CALL(*this, TerminatePidsForUser(_, _))
        .WillByDefault(Return(false));
  }
  virtual ~MockPlatform() {}
  MOCK_METHOD4(Mount, bool(const std::string&, const std::string&,
                           const std::string&, const std::string&));
  MOCK_METHOD2(Bind, bool(const std::string&, const std::string&));
  MOCK_METHOD3(Unmount, bool(const std::string&, bool, bool*));
  MOCK_METHOD1(IsDirectoryMounted, bool(const std::string&));
  MOCK_METHOD2(IsDirectoryMountedWith, bool(const std::string&,
                                            const std::string&));
  MOCK_METHOD2(TerminatePidsWithOpenFiles, bool(const std::string&, bool));
  MOCK_METHOD2(TerminatePidsForUser, bool(const uid_t, bool));
  MOCK_METHOD3(SetOwnership, bool(const std::string&, uid_t, gid_t));
  MOCK_METHOD2(SetPermissions, bool(const std::string&, mode_t));
  MOCK_METHOD3(SetOwnershipRecursive, bool(const std::string&, uid_t, gid_t));
  MOCK_METHOD3(GetUserId, bool(const std::string&, uid_t*, gid_t*));
  MOCK_METHOD2(GetGroupId, bool(const std::string&, gid_t*));
  MOCK_CONST_METHOD1(AmountOfFreeDiskSpace, int64(const std::string&));
  MOCK_METHOD2(Symlink, bool(const std::string&, const std::string&));
  MOCK_METHOD1(FileExists, bool(const std::string&));
  MOCK_METHOD1(DeleteFile, bool(const std::string&));
  MOCK_METHOD2(ReadFile, bool(const std::string&, chromeos::Blob*));
  MOCK_METHOD2(WriteFile, bool(const std::string&, const chromeos::Blob&));

 private:
  bool MockGetUserId(const std::string& user, uid_t* user_id, gid_t* group_id) {
    *user_id = getuid();
    *group_id = getgid();
    return true;
  }
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_PLATFORM_H_
