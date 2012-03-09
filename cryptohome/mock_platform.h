// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

ACTION(CallDeleteFile) { return file_util::Delete(FilePath(arg0), arg1); }
ACTION(CallEnumerateDirectoryEntries) {
  // Pass a call to EnumerateDirectoryEntries through to a real Platform if it's
  // not mocked.
  Platform p;
  return p.EnumerateDirectoryEntries(arg0, arg1, arg2);
}
ACTION(CallDirectoryExists) {
  return file_util::DirectoryExists(FilePath(arg0));
}
ACTION(CallReadFile) {
  return Platform().ReadFile(arg0, arg1);
}

class MockPlatform : public Platform {
 public:
  MockPlatform() {
    ON_CALL(*this, SetOwnership(_, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, SetGroupAccessible(_, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, GetUserId(_, _, _))
        .WillByDefault(Invoke(this, &MockPlatform::MockGetUserId));
    ON_CALL(*this, GetGroupId(_, _))
        .WillByDefault(Invoke(this, &MockPlatform::MockGetGroupId));
    ON_CALL(*this, TerminatePidsWithOpenFiles(_, _))
        .WillByDefault(Return(false));
    ON_CALL(*this, TerminatePidsForUser(_, _))
        .WillByDefault(Return(false));
    ON_CALL(*this, GetCurrentTime())
        .WillByDefault(Return(base::Time::NowFromSystemTime()));
    ON_CALL(*this, DeleteFile(_, _))
        .WillByDefault(CallDeleteFile());
    ON_CALL(*this, EnumerateDirectoryEntries(_, _, _))
        .WillByDefault(CallEnumerateDirectoryEntries());
    ON_CALL(*this, DirectoryExists(_))
        .WillByDefault(CallDirectoryExists());
    ON_CALL(*this, ReadFile(_, _))
        .WillByDefault(CallReadFile());
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
  MOCK_CONST_METHOD3(SetOwnership, bool(const std::string&, uid_t, gid_t));
  MOCK_CONST_METHOD2(SetPermissions, bool(const std::string&, mode_t));
  MOCK_CONST_METHOD3(SetOwnershipRecursive, bool(const std::string&,
                                                 uid_t, gid_t));
  MOCK_CONST_METHOD3(SetGroupAccessible, bool(const std::string&,
                                              gid_t group_id,
                                              mode_t group_mode));
  MOCK_CONST_METHOD3(GetUserId, bool(const std::string&, uid_t*, gid_t*));
  MOCK_CONST_METHOD2(GetGroupId, bool(const std::string&, gid_t*));
  MOCK_CONST_METHOD1(AmountOfFreeDiskSpace, int64(const std::string&));
  MOCK_METHOD2(Symlink, bool(const std::string&, const std::string&));
  MOCK_METHOD1(FileExists, bool(const std::string&));
  MOCK_METHOD2(Stat, bool(const std::string&, struct stat*));
  MOCK_METHOD2(ReadFile, bool(const std::string&, chromeos::Blob*));
  MOCK_METHOD2(WriteFile, bool(const std::string&, const chromeos::Blob&));
  MOCK_CONST_METHOD0(GetCurrentTime, base::Time());
  MOCK_METHOD3(EnumerateDirectoryEntries, bool(const std::string&, bool,
                                               std::vector<std::string>*));
  MOCK_METHOD2(DeleteFile, bool(const std::string&, bool));
  MOCK_METHOD1(DirectoryExists, bool(const std::string&));

 private:
  bool MockGetUserId(const std::string& user, uid_t* user_id, gid_t* group_id) {
    *user_id = getuid();
    *group_id = getgid();
    return true;
  }

  bool MockGetGroupId(const std::string& group, gid_t* group_id) {
    *group_id = getgid();
    return true;
  }
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_PLATFORM_H_
