// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_PLATFORM_H_
#define CRYPTOHOME_MOCK_PLATFORM_H_

#include <base/file_util.h>
#include <base/time.h>
#include <gmock/gmock.h>

#include "platform.h"

namespace chromeos {
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
ACTION(CallPathExists) {
  return file_util::PathExists(FilePath(arg0));
}
ACTION(CallCreateDirectory) {
  return file_util::CreateDirectory(FilePath(arg0));
}
ACTION(CallReadFile) { return Platform().ReadFile(arg0, arg1); }
ACTION(CallReadFileToString) { return Platform().ReadFileToString(arg0, arg1); }
ACTION(CallCopy) { return Platform().Copy(arg0, arg1); }
ACTION(CallRename) { return Platform().Rename(arg0, arg1); }

class MockPlatform : public Platform {
 public:
  MockPlatform() {
    ON_CALL(*this, GetOwnership(_, _, _))
        .WillByDefault(Invoke(this, &MockPlatform::MockGetOwnership));
    ON_CALL(*this, SetOwnership(_, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, GetPermissions(_, _))
        .WillByDefault(Invoke(this, &MockPlatform::MockGetPermissions));
    ON_CALL(*this, SetPermissions(_, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, SetGroupAccessible(_, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, GetUserId(_, _, _))
        .WillByDefault(Invoke(this, &MockPlatform::MockGetUserId));
    ON_CALL(*this, GetGroupId(_, _))
        .WillByDefault(Invoke(this, &MockPlatform::MockGetGroupId));
    ON_CALL(*this, GetCurrentTime())
        .WillByDefault(Return(base::Time::NowFromSystemTime()));
    ON_CALL(*this, Copy(_, _))
        .WillByDefault(CallCopy());
    ON_CALL(*this, DeleteFile(_, _))
        .WillByDefault(CallDeleteFile());
    ON_CALL(*this, EnumerateDirectoryEntries(_, _, _))
        .WillByDefault(CallEnumerateDirectoryEntries());
    ON_CALL(*this, DirectoryExists(_))
        .WillByDefault(CallDirectoryExists());
    ON_CALL(*this, FileExists(_))
        .WillByDefault(CallPathExists());
    ON_CALL(*this, CreateDirectory(_))
      .WillByDefault(CallCreateDirectory());
    ON_CALL(*this, ReadFile(_, _))
        .WillByDefault(CallReadFile());
    ON_CALL(*this, ReadFileToString(_, _))
        .WillByDefault(CallReadFileToString());
    ON_CALL(*this, Rename(_, _))
        .WillByDefault(CallRename());
  }
  virtual ~MockPlatform() {}
  MOCK_METHOD4(Mount, bool(const std::string&, const std::string&,
                           const std::string&, const std::string&));
  MOCK_METHOD2(Bind, bool(const std::string&, const std::string&));
  MOCK_METHOD3(Unmount, bool(const std::string&, bool, bool*));
  MOCK_METHOD1(IsDirectoryMounted, bool(const std::string&));
  MOCK_METHOD2(IsDirectoryMountedWith, bool(const std::string&,
                                            const std::string&));
  MOCK_CONST_METHOD3(GetOwnership, bool(const std::string&, uid_t*, gid_t*));
  MOCK_CONST_METHOD3(SetOwnership, bool(const std::string&, uid_t, gid_t));
  MOCK_CONST_METHOD2(GetPermissions, bool(const std::string&, mode_t*));
  MOCK_CONST_METHOD2(SetPermissions, bool(const std::string&, mode_t));
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
  MOCK_METHOD2(ReadFileToString, bool(const std::string&, std::string*));
  MOCK_METHOD2(Rename, bool(const std::string&, const std::string&));
  MOCK_METHOD2(WriteFile, bool(const std::string&, const chromeos::Blob&));
  MOCK_CONST_METHOD0(GetCurrentTime, base::Time());
  MOCK_METHOD2(Copy, bool(const std::string&, const std::string&));
  MOCK_METHOD3(EnumerateDirectoryEntries, bool(const std::string&, bool,
                                               std::vector<std::string>*));
  MOCK_METHOD2(DeleteFile, bool(const std::string&, bool));
  MOCK_METHOD1(DirectoryExists, bool(const std::string&));
  MOCK_METHOD1(CreateDirectory, bool(const std::string&));

 private:
  bool MockGetOwnership(const std::string& path, uid_t* user_id,
                        gid_t* group_id) const {
    *user_id = getuid();
    *group_id = getgid();
    return true;
  }

  bool MockGetPermissions(const std::string& path, mode_t* mode) const {
    *mode = S_IRWXU | S_IRGRP | S_IXGRP;
    return true;
  }

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
}  // namespace chromeos

#endif  // CRYPTOHOME_MOCK_PLATFORM_H_
