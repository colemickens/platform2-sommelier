// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_PLATFORM_H_
#define CRYPTOHOME_MOCK_PLATFORM_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/time/time.h>
#include <gmock/gmock.h>

#include "cryptohome/platform.h"

namespace cryptohome {

class MockFileEnumerator : public FileEnumerator {
 public:
  MockFileEnumerator() {
    ON_CALL(*this, Next())
        .WillByDefault(::testing::Invoke(this, &MockFileEnumerator::MockNext));
    ON_CALL(*this, GetInfo())
        .WillByDefault(
            ::testing::Invoke(this, &MockFileEnumerator::MockGetInfo));
  }
  virtual ~MockFileEnumerator() {}

  void AddFileEntry(base::FilePath path) {
    // Add a placeholder struct stat.
    struct stat s;
    entries_.emplace_back(FileInfo(path, s));
  }

  MOCK_METHOD0(Next, base::FilePath(void));
  MOCK_METHOD0(GetInfo, FileInfo(void));

  std::vector<FileInfo> entries_;

 protected:
  virtual base::FilePath MockNext() {
    if (entries_.empty())
      return base::FilePath();
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
ACTION(CallReadFileToSecureBlob) {
  return Platform().ReadFileToSecureBlob(arg0, arg1);
}
ACTION(CallCopy) { return Platform().Copy(arg0, arg1); }
ACTION(CallRename) { return Platform().Rename(arg0, arg1); }
ACTION(CallComputeDirectorySize) {
  return Platform().ComputeDirectorySize(arg0);
}
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
  MOCK_METHOD5(Mount, bool(const base::FilePath&, const base::FilePath&,
                           const std::string&, uint32_t, const std::string&));
  MOCK_METHOD2(Bind, bool(const base::FilePath&, const base::FilePath&));
  MOCK_METHOD3(Unmount, bool(const base::FilePath&, bool, bool*));
  MOCK_METHOD1(LazyUnmount, void(const base::FilePath&));
  MOCK_METHOD1(GetLoopDeviceMounts, bool(
      std::multimap<const base::FilePath, const base::FilePath>*));
  MOCK_METHOD2(GetMountsBySourcePrefix, bool(const base::FilePath&,
                  std::multimap<const base::FilePath, const base::FilePath>*));
  MOCK_METHOD1(IsDirectoryMounted, bool(const base::FilePath&));
  MOCK_METHOD2(GetProcessesWithOpenFiles, void(const base::FilePath&,
                                          std::vector<ProcessInformation>*));
  MOCK_CONST_METHOD4(GetOwnership,
                     bool(const base::FilePath&, uid_t*, gid_t*, bool));
  MOCK_CONST_METHOD4(SetOwnership,
                     bool(const base::FilePath&, uid_t, gid_t, bool));
  MOCK_CONST_METHOD2(GetPermissions, bool(const base::FilePath&, mode_t*));
  MOCK_CONST_METHOD2(SetPermissions, bool(const base::FilePath&, mode_t));
  MOCK_CONST_METHOD3(SetGroupAccessible, bool(const base::FilePath&,
                                              gid_t group_id,
                                              mode_t group_mode));
  MOCK_CONST_METHOD1(SetMask, int(int));
  MOCK_CONST_METHOD3(GetUserId, bool(const std::string&, uid_t*, gid_t*));
  MOCK_CONST_METHOD2(GetGroupId, bool(const std::string&, gid_t*));
  MOCK_CONST_METHOD1(AmountOfFreeDiskSpace, int64_t(const base::FilePath&));
  MOCK_CONST_METHOD2(GetQuotaCurrentSpaceForUid,
                     int64_t(const base::FilePath&, uid_t));
  MOCK_CONST_METHOD2(GetQuotaCurrentSpaceForGid,
                     int64_t(const base::FilePath&, gid_t));
  MOCK_METHOD2(Symlink, bool(const base::FilePath&, const base::FilePath&));
  MOCK_METHOD1(FileExists, bool(const base::FilePath&));
  MOCK_METHOD2(GetFileSize, bool(const base::FilePath&, int64_t*));
  MOCK_METHOD1(ComputeDirectorySize, int64_t(const base::FilePath&));
  MOCK_METHOD2(OpenFile, FILE*(const base::FilePath&, const char*));
  MOCK_METHOD3(InitializeFile,
               void(base::File*, const base::FilePath&, uint32_t));
  MOCK_METHOD1(LockFile, bool(int));
  MOCK_METHOD1(CloseFile, bool(FILE*));  // NOLINT(readability/function)
  MOCK_METHOD1(CreateAndOpenTemporaryFile, FILE*(base::FilePath*));
  MOCK_METHOD2(Stat, bool(const base::FilePath&, struct stat*));
  MOCK_METHOD2(HasExtendedFileAttribute,
               bool(const base::FilePath&, const std::string&));
  MOCK_METHOD2(ListExtendedFileAttributes,
               bool(const base::FilePath&, std::vector<std::string>*));
  MOCK_METHOD3(GetExtendedFileAttributeAsString,
               bool(const base::FilePath&, const std::string&, std::string*));
  MOCK_METHOD4(
      GetExtendedFileAttribute,
      bool(const base::FilePath&, const std::string&, char*, ssize_t size));
  MOCK_METHOD4(
      SetExtendedFileAttribute,
      bool(const base::FilePath&, const std::string&, const char*, size_t));
  MOCK_METHOD2(RemoveExtendedFileAttribute,
               bool(const base::FilePath&, const std::string&));
  MOCK_METHOD2(GetExtFileAttributes, bool(const base::FilePath&, int*));
  MOCK_METHOD2(SetExtFileAttributes, bool(const base::FilePath&, int));
  MOCK_METHOD1(HasNoDumpFileAttribute, bool(const base::FilePath&));
  MOCK_METHOD2(ReadFile, bool(const base::FilePath&, brillo::Blob*));
  MOCK_METHOD2(ReadFileToString, bool(const base::FilePath&, std::string*));
  MOCK_METHOD2(ReadFileToSecureBlob,
               bool(const base::FilePath&, brillo::SecureBlob*));
  MOCK_METHOD2(Rename, bool(const base::FilePath&, const base::FilePath&));
  MOCK_METHOD2(WriteOpenFile, bool(FILE*, const brillo::Blob&));
  MOCK_METHOD2(WriteFile, bool(const base::FilePath&, const brillo::Blob&));
  MOCK_METHOD2(WriteSecureBlobToFile, bool(const base::FilePath&,
                                           const brillo::SecureBlob&));
  MOCK_METHOD3(WriteFileAtomic, bool(const base::FilePath&,
                                     const brillo::Blob&,
                                     mode_t mode));
  MOCK_METHOD3(WriteSecureBlobToFileAtomic, bool(const base::FilePath&,
                                                 const brillo::SecureBlob&,
                                                 mode_t mode));
  MOCK_METHOD3(WriteFileAtomicDurable, bool(const base::FilePath&,
                                            const brillo::Blob&,
                                            mode_t mode));
  MOCK_METHOD3(WriteSecureBlobToFileAtomicDurable,
               bool(const base::FilePath&,
                    const brillo::SecureBlob&,
                    mode_t mode));
  MOCK_METHOD2(WriteStringToFile, bool(const base::FilePath&,
                                       const std::string&));
  MOCK_METHOD3(WriteStringToFileAtomicDurable, bool(const base::FilePath&,
                                                    const std::string&,
                                                    mode_t mode));
  MOCK_METHOD3(WriteArrayToFile, bool(const base::FilePath& path,
                                      const char* data,
                                      size_t size));
  MOCK_METHOD1(TouchFileDurable, bool(const base::FilePath& path));
  MOCK_CONST_METHOD0(GetCurrentTime, base::Time());
  MOCK_METHOD2(Copy, bool(const base::FilePath&, const base::FilePath&));
  MOCK_METHOD2(Move, bool(const base::FilePath&, const base::FilePath&));
  MOCK_METHOD2(StatVFS, bool(const base::FilePath&, struct statvfs*));
  MOCK_METHOD2(ReportFilesystemDetails, bool(const base::FilePath&,
                                             const base::FilePath&));
  MOCK_METHOD2(FindFilesystemDevice, bool(const base::FilePath&,
                                          std::string*));
  MOCK_METHOD3(EnumerateDirectoryEntries, bool(const base::FilePath&, bool,
                                               std::vector<base::FilePath>*));
  MOCK_METHOD2(DeleteFile, bool(const base::FilePath&, bool));
  MOCK_METHOD2(DeleteFileDurable, bool(const base::FilePath&, bool));
  MOCK_METHOD1(DirectoryExists, bool(const base::FilePath&));
  MOCK_METHOD1(CreateDirectory, bool(const base::FilePath&));
  MOCK_METHOD0(SetupProcessKeyring, bool());
  MOCK_METHOD1(GetDirCryptoKeyState,
               dircrypto::KeyState(const base::FilePath&));
  MOCK_METHOD2(SetDirCryptoKey, bool(const base::FilePath&,
                                     const brillo::SecureBlob&));
  MOCK_METHOD3(AddDirCryptoKeyToKeyring, bool(const brillo::SecureBlob&,
                                              const brillo::SecureBlob&,
                                              key_serial_t*));
  MOCK_METHOD2(InvalidateDirCryptoKey, bool(key_serial_t,
                                            const base::FilePath&));
  MOCK_METHOD0(ClearUserKeyring, bool(void));
  MOCK_METHOD3(AddEcryptfsAuthToken,
               bool(const brillo::SecureBlob&,
                    const std::string&,
                    const brillo::SecureBlob&));
  MOCK_METHOD3(GetFileEnumerator, FileEnumerator*(const base::FilePath&,
                                                  bool,
                                                  int));
  MOCK_METHOD0(FirmwareWriteProtected, bool(void));
  MOCK_METHOD1(DataSyncFile, bool(const base::FilePath&));
  MOCK_METHOD1(SyncFile, bool(const base::FilePath&));
  MOCK_METHOD1(SyncDirectory, bool(const base::FilePath&));
  MOCK_METHOD0(Sync, void());
  MOCK_METHOD0(GetHardwareID, std::string(void));
  MOCK_METHOD2(CreateSymbolicLink,
               bool(const base::FilePath&, const base::FilePath&));
  MOCK_METHOD2(ReadLink, bool(const base::FilePath&, base::FilePath*));
  MOCK_METHOD4(SetFileTimes,
               bool(const base::FilePath&,
                    const struct timespec&,
                    const struct timespec&,
                    bool));
  MOCK_METHOD4(SendFile, bool(int, int, off_t, size_t));
  MOCK_METHOD2(CreateSparseFile, bool(const base::FilePath&, size_t));
  MOCK_METHOD1(AttachLoop, base::FilePath(const base::FilePath&));
  MOCK_METHOD1(DetachLoop, bool(const base::FilePath&));
  MOCK_METHOD0(GetAttachedLoopDevices, std::vector<LoopDevice>());
  MOCK_METHOD1(FormatExt4, bool(const base::FilePath&));
  MOCK_METHOD2(RestoreSELinuxContexts, bool(const base::FilePath&, bool));

  MockFileEnumerator* mock_enumerator() { return mock_enumerator_.get(); }

 private:
  bool MockGetOwnership(const base::FilePath& path,
                        uid_t* user_id,
                        gid_t* group_id,
                        bool dereference_links) const {
    *user_id = getuid();
    *group_id = getgid();
    return true;
  }

  bool MockGetPermissions(const base::FilePath& path, mode_t* mode) const {
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

  FileEnumerator* MockGetFileEnumerator(const base::FilePath& root_path,
                                        bool recursive,
                                        int file_type) {
    MockFileEnumerator* e = mock_enumerator_.release();
    mock_enumerator_.reset(new ::testing::NiceMock<MockFileEnumerator>());
    mock_enumerator_->entries_.assign(e->entries_.begin(), e->entries_.end());
    return e;
  }
  std::unique_ptr<MockFileEnumerator> mock_enumerator_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_PLATFORM_H_
