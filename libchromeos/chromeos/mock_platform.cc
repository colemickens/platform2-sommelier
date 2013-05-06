// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_platform.h"

namespace chromeos {

MockPlatform::MockPlatform() : mock_enumerator_(new MockFileEnumerator()) {
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
  ON_CALL(*this, GetFileEnumerator(_, _, _))
      .WillByDefault(Invoke(this, &MockPlatform::MockGetFileEnumerator));
  ON_CALL(*this, GetCurrentTime())
      .WillByDefault(Return(base::Time::NowFromSystemTime()));
  ON_CALL(*this, Copy(_, _))
      .WillByDefault(CallCopy());
  ON_CALL(*this, GetFilesystemStats(_, _))
      .WillByDefault(CallGetFilesystemStats());
  ON_CALL(*this, FindFilesystemDevice(_, _))
      .WillByDefault(CallFindFilesystemDevice());
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

MockPlatform::~MockPlatform() {}

}  // namespace chromeos
