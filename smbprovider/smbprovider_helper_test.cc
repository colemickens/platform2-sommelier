// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <gtest/gtest.h>
#include <libsmbclient.h>

#include "smbprovider/constants.h"
#include "smbprovider/proto.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/smbprovider_helper.h"
#include "smbprovider/smbprovider_test_helper.h"
#include "smbprovider/temp_file_manager.h"

namespace smbprovider {

class SmbProviderHelperTest : public testing::Test {
 public:
  SmbProviderHelperTest() = default;
  ~SmbProviderHelperTest() override = default;

 protected:
  TempFileManager temp_file_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbProviderHelperTest);
};

// Tests that AppendPath properly appends with or without the trailing separator
// "/" on the base path.
TEST_F(SmbProviderHelperTest, AppendPath) {
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare", "/test"));
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare/", "/test"));
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare", "test"));
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare/", "test"));
  EXPECT_EQ("smb://qnap/testshare", AppendPath("smb://qnap/testshare/", "/"));
  EXPECT_EQ("smb://qnap/testshare", AppendPath("smb://qnap/testshare/", ""));
}

// Should return true on "." and ".." entries.
TEST_F(SmbProviderHelperTest, IsSelfOrParentDir) {
  EXPECT_TRUE(IsSelfOrParentDir("."));
  EXPECT_TRUE(IsSelfOrParentDir(".."));
  EXPECT_FALSE(IsSelfOrParentDir("/"));
  EXPECT_FALSE(IsSelfOrParentDir("test.jpg"));
}

// Only SMBC_DIR and SMBC_FILE should return true.
TEST_F(SmbProviderHelperTest, IsFileOrDir) {
  EXPECT_TRUE(IsFileOrDir(SMBC_DIR));
  EXPECT_TRUE(IsFileOrDir(SMBC_FILE));

  EXPECT_FALSE(IsFileOrDir(SMBC_WORKGROUP));
  EXPECT_FALSE(IsFileOrDir(SMBC_SERVER));
  EXPECT_FALSE(IsFileOrDir(SMBC_FILE_SHARE));
  EXPECT_FALSE(IsFileOrDir(SMBC_PRINTER_SHARE));
  EXPECT_FALSE(IsFileOrDir(SMBC_COMMS_SHARE));
  EXPECT_FALSE(IsFileOrDir(SMBC_IPC_SHARE));
  EXPECT_FALSE(IsFileOrDir(SMBC_LINK));
}

// Only SMBC_FILE_SHARE should return true.
TEST_F(SmbProviderHelperTest, IsSmbShare) {
  EXPECT_TRUE(IsSmbShare(SMBC_FILE_SHARE));

  EXPECT_FALSE(IsSmbShare(SMBC_DIR));
  EXPECT_FALSE(IsSmbShare(SMBC_FILE));
  EXPECT_FALSE(IsSmbShare(SMBC_WORKGROUP));
  EXPECT_FALSE(IsSmbShare(SMBC_SERVER));
  EXPECT_FALSE(IsSmbShare(SMBC_PRINTER_SHARE));
  EXPECT_FALSE(IsSmbShare(SMBC_COMMS_SHARE));
  EXPECT_FALSE(IsSmbShare(SMBC_IPC_SHARE));
  EXPECT_FALSE(IsSmbShare(SMBC_LINK));
}

// Errors should be returned correctly.
TEST_F(SmbProviderHelperTest, GetErrorFromErrno) {
  EXPECT_EQ(ERROR_ACCESS_DENIED, GetErrorFromErrno(EPERM));
  EXPECT_EQ(ERROR_ACCESS_DENIED, GetErrorFromErrno(EACCES));

  EXPECT_EQ(ERROR_NOT_FOUND, GetErrorFromErrno(EBADF));
  EXPECT_EQ(ERROR_NOT_FOUND, GetErrorFromErrno(ENODEV));
  EXPECT_EQ(ERROR_NOT_FOUND, GetErrorFromErrno(ENOENT));

  EXPECT_EQ(ERROR_TOO_MANY_OPENED, GetErrorFromErrno(EMFILE));
  EXPECT_EQ(ERROR_TOO_MANY_OPENED, GetErrorFromErrno(ENFILE));

  EXPECT_EQ(ERROR_NOT_A_DIRECTORY, GetErrorFromErrno(ENOTDIR));

  EXPECT_EQ(ERROR_NOT_A_FILE, GetErrorFromErrno(EISDIR));

  EXPECT_EQ(ERROR_NOT_EMPTY, GetErrorFromErrno(ENOTEMPTY));

  EXPECT_EQ(ERROR_EXISTS, GetErrorFromErrno(EEXIST));

  EXPECT_EQ(ERROR_INVALID_OPERATION, GetErrorFromErrno(EINVAL));

  EXPECT_EQ(ERROR_SMB1_UNSUPPORTED, GetErrorFromErrno(ECONNABORTED));

  // Errors without an explicit mapping get mapped
  // to ERROR_FAILED.
  EXPECT_EQ(ERROR_FAILED, GetErrorFromErrno(ENOSPC));
  EXPECT_EQ(ERROR_FAILED, GetErrorFromErrno(ESPIPE));
}

// IsDirectory should only return true on directory stats.
TEST_F(SmbProviderHelperTest, IsDirectory) {
  struct stat dir_info;
  dir_info.st_mode = 16877;  // Dir mode
  struct stat file_info;
  file_info.st_mode = 33188;  // File mode

  EXPECT_TRUE(IsDirectory(dir_info));
  EXPECT_FALSE(IsDirectory(file_info));
}

// IsFile should only return true on File stats.
TEST_F(SmbProviderHelperTest, IsFile) {
  struct stat dir_info;
  dir_info.st_mode = 16877;  // Dir mode
  struct stat file_info;
  file_info.st_mode = 33188;  // File mode

  EXPECT_TRUE(IsFile(file_info));
  EXPECT_FALSE(IsFile(dir_info));
}

// IsValidOpenFileFlags should return true on valid flags.
TEST_F(SmbProviderHelperTest, IsValidOpenFileFlags) {
  EXPECT_TRUE(IsValidOpenFileFlags(O_RDWR));
  EXPECT_TRUE(IsValidOpenFileFlags(O_RDONLY));
  EXPECT_TRUE(IsValidOpenFileFlags(O_WRONLY));
  EXPECT_FALSE(IsValidOpenFileFlags(O_CREAT));
  EXPECT_FALSE(IsValidOpenFileFlags(O_TRUNC));
}

TEST_F(SmbProviderHelperTest, ReadFromFDErrorOnInvalidFd) {
  std::vector<uint8_t> buffer;
  int32_t error;

  WriteFileOptionsProto proto = CreateWriteFileOptionsProto(
      0 /* mount_id */, 1 /* file_id */, 0 /* offset */, 0 /* length */);

  // Create a file descriptor that is invalid.
  base::ScopedFD invalid_fd;
  EXPECT_FALSE(invalid_fd.is_valid());

  // Should return an error when passing in an invalid file descriptor.
  EXPECT_FALSE(ReadFromFD(proto, invalid_fd, &error, &buffer));
  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED, static_cast<ErrorType>(error));
}

TEST_F(SmbProviderHelperTest, ReadFromFDFailsWithLengthLargerThanData) {
  const std::vector<uint8_t> data = {0, 1, 2, 3};

  // Send in options with length larger than the data.
  WriteFileOptionsProto proto = CreateWriteFileOptionsProto(
      0 /* mount_id */, 1 /* file_id */, 0 /* offset */, data.size() + 1);
  int32_t error;

  // Ensure that the fd created is valid.
  base::ScopedFD fd(temp_file_manager_.CreateTempFile(data).release());
  EXPECT_TRUE(fd.is_valid());

  // Should fail since it can't read that much data.
  std::vector<uint8_t> buffer;
  EXPECT_FALSE(ReadFromFD(proto, fd, &error, &buffer));
  EXPECT_EQ(ERROR_IO, static_cast<ErrorType>(error));
}

TEST_F(SmbProviderHelperTest, ReadFromFDSucceedsWithLengthSmallerThanData) {
  const std::vector<uint8_t> data = {0, 1, 2, 3};

  // Send in options with length smaller than the data.
  WriteFileOptionsProto proto = CreateWriteFileOptionsProto(
      0 /* mount_id */, 1 /* file_id */, 0 /* offset */, data.size() - 1);
  int32_t error;

  // Ensure that the fd created is valid.
  base::ScopedFD fd(temp_file_manager_.CreateTempFile(data).release());
  EXPECT_TRUE(fd.is_valid());

  // Should be OK.
  std::vector<uint8_t> buffer;
  EXPECT_TRUE(ReadFromFD(proto, fd, &error, &buffer));

  // Should be equal to the truncated data.
  const std::vector<uint8_t> expected = {0, 1, 2};
  EXPECT_EQ(expected, buffer);
}

TEST_F(SmbProviderHelperTest, ReadFromFDSucceedsWithExactSize) {
  const std::vector<uint8_t> data = {0, 1, 2, 3};

  // Send in options with length equal to the data.
  WriteFileOptionsProto proto = CreateWriteFileOptionsProto(
      0 /* mount_id */, 1 /* file_id */, 0 /* offset */, data.size());
  int32_t error;

  // Ensure that the fd created is valid.
  base::ScopedFD fd(temp_file_manager_.CreateTempFile(data).release());
  EXPECT_TRUE(fd.is_valid());

  // Should be OK.
  std::vector<uint8_t> buffer;
  EXPECT_TRUE(ReadFromFD(proto, fd, &error, &buffer));
  EXPECT_EQ(data, buffer);
}

// SplitPath correctly splits a relative path into a vector of its components.
TEST_F(SmbProviderHelperTest, SplitPathCorrectlySplitsPath) {
  const std::string relative_path = "/testShare/dogs/lab.jpg";

  PathParts parts = SplitPath(relative_path);

  EXPECT_EQ(4, parts.size());
  EXPECT_EQ("/", parts[0]);
  EXPECT_EQ("testShare", parts[1]);
  EXPECT_EQ("dogs", parts[2]);
  EXPECT_EQ("lab.jpg", parts[3]);
}

// SplitPath correctly splits a standalone leading slash and a standalone
// directory.
TEST_F(SmbProviderHelperTest, SplitPathCorrectlySplitsRoot) {
  const std::string root_path = "/";

  PathParts parts = SplitPath(root_path);

  EXPECT_EQ(1, parts.size());
  EXPECT_EQ("/", parts[0]);
}

// SplitPath correctly splits a standalone directory.
TEST_F(SmbProviderHelperTest, SplitPathCorrectlySplitsDirPath) {
  const std::string dir_path = "/foo";

  PathParts parts = SplitPath(dir_path);

  EXPECT_EQ(2, parts.size());
  EXPECT_EQ("/", parts[0]);
  EXPECT_EQ("foo", parts[1]);
}

// RemoveUrlScheme correctly removes the SMB Url scheme from an SMB Url.
TEST_F(SmbProviderHelperTest, RemoveUrlSchemeCorrectlyRemovesUrl) {
  EXPECT_EQ("/testShare/dogs", RemoveURLScheme("smb://testShare/dogs"));
}

// GetFileName correctly returns root when passed "smb://".
TEST_F(SmbProviderHelperTest, GetFileNameReturnsRoot) {
  const std::string full_path = "smb://";

  EXPECT_EQ("/", GetFileName(full_path));
}

// GetFileName correctly returns the filename when passed "smb://foo".
TEST_F(SmbProviderHelperTest, GetFileNameReturnsFileNameOnSingleDepth) {
  const std::string full_path = "smb://foo";

  EXPECT_EQ("foo", GetFileName(full_path));
}

// GetFileName correctly returns the filename from an SMB Url.
TEST_F(SmbProviderHelperTest, GetFileNameReturnsFileName) {
  const std::string full_path = "smb://testShare/dogs/lab.jpg";

  EXPECT_EQ("lab.jpg", GetFileName(full_path));
}

// GetDirPath correctly returns root when passed "smb://".
TEST_F(SmbProviderHelperTest, GetDirPathReturnsRoot) {
  const std::string full_path = "smb://";

  EXPECT_EQ("/", GetDirPath(full_path));
}

// GetDirPath correctly returns the dirpath when passed "smb://foo".
TEST_F(SmbProviderHelperTest, GetDirPathReturnsRootOnSingleDepth) {
  const std::string full_path = "smb://foo";

  EXPECT_EQ("/", GetDirPath(full_path));
}

// GetDirPath correctly returns the dirpath from an SMB Url.
TEST_F(SmbProviderHelperTest, GetDirPathReturnsParent) {
  const std::string full_path = "smb://testShare/dogs/lab.jpg";

  EXPECT_EQ("/testShare/dogs", GetDirPath(full_path));
}

TEST_F(SmbProviderHelperTest, ShouldReportCreateDirError) {
  EXPECT_FALSE(
      ShouldReportCreateDirError(0 /* result */, false /* ignore_existing */));
  EXPECT_FALSE(
      ShouldReportCreateDirError(0 /* result */, true /* ignore_existing */));
  EXPECT_FALSE(ShouldReportCreateDirError(EEXIST, true /* ignore_existing */));
  EXPECT_TRUE(ShouldReportCreateDirError(EEXIST, false /* ignore_existing */));
  EXPECT_TRUE(ShouldReportCreateDirError(EPERM, false /* ignore_existing */));
  EXPECT_TRUE(ShouldReportCreateDirError(EPERM, true /* ignore_existing */));
}

// GetOpenFilePermissions correctly returns permissions.
TEST_F(SmbProviderHelperTest, GetOpenFilePermissions) {
  OpenFileOptionsProto open_file_proto_read = CreateOpenFileOptionsProto(
      3 /* mount_id */, "smb://testShare/dir1/pic.png", false /* writeable */);
  EXPECT_EQ(O_RDONLY, GetOpenFilePermissions(open_file_proto_read));

  OpenFileOptionsProto open_file_proto_read_write = CreateOpenFileOptionsProto(
      3 /* mount_id */, "smb://testShare/dir1/pic.png", true /* writeable */);
  EXPECT_EQ(O_RDWR, GetOpenFilePermissions(open_file_proto_read_write));

  TruncateOptionsProto truncate_proto_blank;
  EXPECT_EQ(O_WRONLY, GetOpenFilePermissions(truncate_proto_blank));

  CopyEntryOptionsProto copy_entry_proto_blank;
  EXPECT_EQ(O_RDONLY, GetOpenFilePermissions(copy_entry_proto_blank));
}

TEST_F(SmbProviderHelperTest, GetOpenFilePermissionsBoolean) {
  EXPECT_EQ(O_RDWR, GetOpenFilePermissions(true));

  EXPECT_EQ(O_RDONLY, GetOpenFilePermissions(false));
}

}  // namespace smbprovider
