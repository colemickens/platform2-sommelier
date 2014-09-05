// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_PLATFORM_H_
#define CRYPTOHOME_MOCK_PLATFORM_H_

#include <map>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/time/time.h>
#include <gmock/gmock.h>

#include "cryptohome/platform.h"

namespace cryptohome {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Invoke;
using ::testing::Return;

class MockFileEnumerator : public FileEnumerator {
 public:
  MockFileEnumerator() {
    ON_CALL(*this, Next())
      .WillByDefault(Invoke(this, &MockFileEnumerator::MockNext));
    ON_CALL(*this, GetInfo())
      .WillByDefault(Invoke(this, &MockFileEnumerator::MockGetInfo));
  }
  virtual ~MockFileEnumerator() {}

  MOCK_METHOD0(Next, std::string(void));
  MOCK_METHOD0(GetInfo, FileInfo(void));

  std::vector<FileInfo> entries_;

 protected:
  virtual std::string MockNext() {
    if (entries_.empty())
      return std::string();
    current_ = entries_.at(0);
    entries_.erase(entries_.begin(), entries_.begin()+1);
    return current_.GetName();
  }
  virtual const FileInfo& MockGetInfo() {
    return current_;
  }
  FileInfo current_;
};

// TODO(wad) Migrate to an in-memory-only mock filesystem.
ACTION(CallDeleteFile) { return base::DeleteFile(base::FilePath(arg0), arg1); }
ACTION(CallEnumerateDirectoryEntries) {
  // Pass a call to EnumerateDirectoryEntries through to a real Platform if it's
  // not mocked.
  Platform p;
  return p.EnumerateDirectoryEntries(arg0, arg1, arg2);
}
ACTION(CallDirectoryExists) {
  return base::DirectoryExists(base::FilePath(arg0));
}
ACTION(CallPathExists) {
  return base::PathExists(base::FilePath(arg0));
}
ACTION(CallCreateDirectory) {
  return base::CreateDirectory(base::FilePath(arg0));
}
ACTION(CallReadFile) { return Platform().ReadFile(arg0, arg1); }
ACTION(CallReadFileToString) { return Platform().ReadFileToString(arg0, arg1); }
ACTION(CallCopy) { return Platform().Copy(arg0, arg1); }
ACTION(CallRename) { return Platform().Rename(arg0, arg1); }
ACTION(CallStatVFS) { return Platform().StatVFS(arg0, arg1); }
ACTION(CallReportFilesystemDetails) {
  return Platform().ReportFilesystemDetails(arg0, arg1);
}
ACTION(CallFindFilesystemDevice) {
  return Platform().FindFilesystemDevice(arg0, arg1);
}

class MockPlatform : public Platform {
 public:
  MockPlatform();
  virtual ~MockPlatform();
  MOCK_METHOD4(Mount, bool(const std::string&, const std::string&,
                           const std::string&, const std::string&));
  MOCK_METHOD2(Bind, bool(const std::string&, const std::string&));
  MOCK_METHOD3(Unmount, bool(const std::string&, bool, bool*));
  MOCK_METHOD2(LazyUnmountAndSync, void(const std::string&, bool));
  MOCK_METHOD2(GetMountsBySourcePrefix, bool(const std::string&,
                  std::multimap<const std::string, const std::string>*));
  MOCK_METHOD1(IsDirectoryMounted, bool(const std::string&));
  MOCK_METHOD2(IsDirectoryMountedWith, bool(const std::string&,
                                            const std::string&));
  MOCK_METHOD2(GetProcessesWithOpenFiles, void(const std::string&,
                                          std::vector<ProcessInformation>*));
  MOCK_CONST_METHOD3(GetOwnership, bool(const std::string&, uid_t*, gid_t*));
  MOCK_CONST_METHOD3(SetOwnership, bool(const std::string&, uid_t, gid_t));
  MOCK_CONST_METHOD2(GetPermissions, bool(const std::string&, mode_t*));
  MOCK_CONST_METHOD2(SetPermissions, bool(const std::string&, mode_t));
  MOCK_CONST_METHOD3(SetGroupAccessible, bool(const std::string&,
                                              gid_t group_id,
                                              mode_t group_mode));
  MOCK_CONST_METHOD1(SetMask, int(int));
  MOCK_CONST_METHOD3(GetUserId, bool(const std::string&, uid_t*, gid_t*));
  MOCK_CONST_METHOD2(GetGroupId, bool(const std::string&, gid_t*));
  MOCK_CONST_METHOD1(AmountOfFreeDiskSpace, int64_t(const std::string&));
  MOCK_METHOD2(Symlink, bool(const std::string&, const std::string&));
  MOCK_METHOD1(FileExists, bool(const std::string&));
  MOCK_METHOD2(GetFileSize, bool(const std::string&, int64_t*));
  MOCK_METHOD2(OpenFile, FILE*(const std::string&, const char*));
  MOCK_METHOD1(CloseFile, bool(FILE*));  // NOLINT(readability/function)
  MOCK_METHOD1(CreateAndOpenTemporaryFile, FILE*(std::string*));
  MOCK_METHOD2(Stat, bool(const std::string&, struct stat*));
  MOCK_METHOD2(ReadFile, bool(const std::string&, chromeos::Blob*));
  MOCK_METHOD2(ReadFileToString, bool(const std::string&, std::string*));
  MOCK_METHOD2(Rename, bool(const std::string&, const std::string&));
  MOCK_METHOD2(WriteOpenFile, bool(FILE*, const chromeos::Blob&));
  MOCK_METHOD2(WriteFile, bool(const std::string&, const chromeos::Blob&));
  MOCK_METHOD2(WriteStringToFile, bool(const std::string&, const std::string&));
  MOCK_METHOD3(WriteArrayToFile, bool(const std::string& path, const char* data,
                                      size_t size));
  MOCK_CONST_METHOD0(GetCurrentTime, base::Time());
  MOCK_METHOD2(Copy, bool(const std::string&, const std::string&));
  MOCK_METHOD2(Move, bool(const std::string&, const std::string&));
  MOCK_METHOD2(StatVFS, bool(const std::string&, struct statvfs*));
  MOCK_METHOD2(ReportFilesystemDetails, bool(const std::string&,
                                             const std::string&));
  MOCK_METHOD2(FindFilesystemDevice, bool(const std::string&,
                                          std::string*));
  MOCK_METHOD3(EnumerateDirectoryEntries, bool(const std::string&, bool,
                                               std::vector<std::string>*));
  MOCK_METHOD2(DeleteFile, bool(const std::string&, bool));
  MOCK_METHOD1(DirectoryExists, bool(const std::string&));
  MOCK_METHOD1(CreateDirectory, bool(const std::string&));
  MOCK_METHOD0(ClearUserKeyring, long(void));  // NOLINT(runtime/int)
  MOCK_METHOD3(AddEcryptfsAuthToken,
               long(const chromeos::SecureBlob&,  // NOLINT(runtime/int)
                    const std::string&,
                    const chromeos::SecureBlob&));
  MOCK_METHOD3(GetFileEnumerator, FileEnumerator*(const std::string&,
                                                  bool,
                                                  int));
  MOCK_METHOD0(FirmwareWriteProtected, bool(void));
  MOCK_METHOD1(SyncFile, bool(const std::string&));
  MOCK_METHOD0(Sync, void());

  MockFileEnumerator* mock_enumerator() { return mock_enumerator_.get(); }

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

  FileEnumerator* MockGetFileEnumerator(const std::string& root_path,
                                        bool recursive,
                                        int file_type) {
    MockFileEnumerator* e = mock_enumerator_.release();
    mock_enumerator_.reset(new NiceMock<MockFileEnumerator>());
    mock_enumerator_->entries_.assign(e->entries_.begin(), e->entries_.end());
    return e;
  }
  scoped_ptr<MockFileEnumerator> mock_enumerator_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_PLATFORM_H_
