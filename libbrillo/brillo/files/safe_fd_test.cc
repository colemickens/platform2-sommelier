// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brillo/files/safe_fd.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/syslog_logging.h>
#include <gtest/gtest.h>

namespace brillo {
namespace {

#define TO_STRING_HELPER(x)      \
  case brillo::SafeFD::Error::x: \
    return #x;

std::string to_string(brillo::SafeFD::Error err) {
  switch (err) {
    TO_STRING_HELPER(kNoError)
    TO_STRING_HELPER(kBadArgument)
    TO_STRING_HELPER(kNotInitialized)
    TO_STRING_HELPER(kIOError)
    TO_STRING_HELPER(kDoesNotExist)
    TO_STRING_HELPER(kSymlinkDetected)
    TO_STRING_HELPER(kWrongType)
    TO_STRING_HELPER(kWrongUID)
    TO_STRING_HELPER(kWrongGID)
    TO_STRING_HELPER(kWrongPermissions)
    TO_STRING_HELPER(kExceededMaximum)
    default:
      return std::string("unknown (") + std::to_string(static_cast<int>(err)) +
             ")";
  }
}

#define EXPECT_STR_EQ(a, b) EXPECT_EQ(to_string(a), to_string(b))

std::string GetRandomSuffix() {
  const int kBufferSize = 6;
  unsigned char buffer[kBufferSize];
  base::RandBytes(buffer, arraysize(buffer));
  return base::HexEncode(buffer, arraysize(buffer));
}

}  // namespace

class SafeFDTest : public testing::Test {
 public:
  static constexpr char kFileName[] = "test.temp";
  static constexpr char kSubdirName[] = "test_dir";
  static constexpr char kSymbolicFileName[] = "sym_test.temp";
  static constexpr char kSymbolicDirName[] = "sym_dir";

  static void SetUpTestCase() {
    brillo::InitLog(brillo::kLogToStderr);
    umask(0);
  }

  SafeFDTest() {
    CHECK(temp_dir_.CreateUniqueTempDir()) << strerror(errno);
    sub_dir_path_ = temp_dir_.GetPath().Append(kSubdirName);
    file_path_ = sub_dir_path_.Append(kFileName);

    std::string path = temp_dir_.GetPath().value();
    temp_dir_path_.reserve(path.size() + 1);
    temp_dir_path_.assign(temp_dir_.GetPath().value().begin(),
                          temp_dir_.GetPath().value().end());
    temp_dir_path_.push_back('\0');

    CHECK_EQ(chmod(temp_dir_path_.data(), SafeFD::kDefaultDirPermissions), 0);
    SafeFD::SetRootPathForTesting(temp_dir_path_.data());
    root_ = SafeFD::Root().first;
    CHECK(root_.is_valid());
  }

 protected:
  std::vector<char> temp_dir_path_;
  base::FilePath file_path_;
  base::FilePath sub_dir_path_;
  base::FilePath symlink_file_path_;
  base::FilePath symlink_dir_path_;
  base::ScopedTempDir temp_dir_;
  SafeFD root_;

  bool SetupSubdir() WARN_UNUSED_RESULT {
    if (!base::CreateDirectory(sub_dir_path_)) {
      PLOG(ERROR) << "Failed to create '" << sub_dir_path_.value() << "'";
      return false;
    }
    if (chmod(sub_dir_path_.value().c_str(), SafeFD::kDefaultDirPermissions) !=
        0) {
      PLOG(ERROR) << "Failed to set permissions of '" << sub_dir_path_.value()
                  << "'";
      return false;
    }
    return true;
  }

  bool SetupSymlinks() WARN_UNUSED_RESULT {
    symlink_file_path_ = temp_dir_.GetPath().Append(kSymbolicFileName);
    symlink_dir_path_ = temp_dir_.GetPath().Append(kSymbolicDirName);
    if (!base::CreateSymbolicLink(file_path_, symlink_file_path_)) {
      PLOG(ERROR) << "Failed to create symlink to '"
                  << symlink_file_path_.value() << "'";
      return false;
    }
    if (!base::CreateSymbolicLink(temp_dir_.GetPath(), symlink_dir_path_)) {
      PLOG(ERROR) << "Failed to create symlink to'" << symlink_dir_path_.value()
                  << "'";
      return false;
    }
    return true;
  }

  // Writes |contents| to |file_path_|. Pulled into a separate function just
  // to improve readability of tests.
  bool WriteFile(const std::string& contents) WARN_UNUSED_RESULT {
    if (!SetupSubdir()) {
      return false;
    }
    if (contents.length() !=
        base::WriteFile(file_path_, contents.c_str(), contents.length())) {
      PLOG(ERROR) << "base::WriteFile failed";
      return false;
    }
    if (chmod(file_path_.value().c_str(), SafeFD::kDefaultFilePermissions) !=
        0) {
      PLOG(ERROR) << "chmod failed";
      return false;
    }
    return true;
  }

  // Verifies that the file at |file_path_| exists and contains |contents|.
  void ExpectFileContains(const std::string& contents) {
    EXPECT_TRUE(base::PathExists(file_path_));
    std::string new_contents;
    EXPECT_TRUE(base::ReadFileToString(file_path_, &new_contents));
    EXPECT_EQ(contents, new_contents);
  }

  // Verifies that the file at |file_path_| has |permissions|.
  void ExpectPermissions(base::FilePath path, int permissions) {
    int actual_permissions = 0;
    // This breaks out of the ExpectPermissions() call but not the test case.
    ASSERT_TRUE(base::GetPosixFilePermissions(path, &actual_permissions));
    EXPECT_EQ(permissions, actual_permissions);
  }

  // Creates a file with a random name in the temporary directory.
  base::FilePath GetTempName() {
    return temp_dir_.GetPath().Append(GetRandomSuffix());
  }
};

constexpr char SafeFDTest::kFileName[];
constexpr char SafeFDTest::kSubdirName[];
constexpr char SafeFDTest::kSymbolicFileName[];
constexpr char SafeFDTest::kSymbolicDirName[];

TEST_F(SafeFDTest, SafeFD) {
  EXPECT_FALSE(SafeFD().is_valid());
}

TEST_F(SafeFDTest, SafeFD_Move) {
  SafeFD moved_root = std::move(root_);
  EXPECT_FALSE(root_.is_valid());
  ASSERT_TRUE(moved_root.is_valid());

  SafeFD moved_root2(std::move(moved_root));
  EXPECT_FALSE(moved_root.is_valid());
  ASSERT_TRUE(moved_root2.is_valid());
}

TEST_F(SafeFDTest, Root) {
  SafeFD::SafeFDResult result = SafeFD::Root();
  EXPECT_TRUE(result.first.is_valid());
  EXPECT_STR_EQ(result.second, SafeFD::Error::kNoError);
}

TEST_F(SafeFDTest, reset) {
  root_.reset();
  EXPECT_FALSE(root_.is_valid());
}

TEST_F(SafeFDTest, UnsafeReset) {
  int fd =
      HANDLE_EINTR(open(temp_dir_path_.data(),
                        O_NONBLOCK | O_DIRECTORY | O_RDONLY | O_CLOEXEC, 0777));
  ASSERT_GE(fd, 0);

  {
    SafeFD safe_fd;
    safe_fd.UnsafeReset(fd);
    EXPECT_EQ(safe_fd.get(), fd);
  }

  // Verify the file descriptor is closed.
  int result = fcntl(fd, F_GETFD);
  int error = errno;
  EXPECT_EQ(result, -1);
  EXPECT_EQ(error, EBADF);
}

TEST_F(SafeFDTest, Write_Success) {
  std::string random_data = GetRandomSuffix();
  {
    SafeFD::SafeFDResult file = root_.MakeFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
    ASSERT_TRUE(file.first.is_valid());

    EXPECT_STR_EQ(file.first.Write(random_data.data(), random_data.size()),
                  SafeFD::Error::kNoError);
  }

  ExpectFileContains(random_data);
  ExpectPermissions(file_path_, SafeFD::kDefaultFilePermissions);
}

TEST_F(SafeFDTest, Write_NotInitialized) {
  SafeFD invalid;
  ASSERT_FALSE(invalid.is_valid());

  std::string random_data = GetRandomSuffix();
  EXPECT_STR_EQ(invalid.Write(random_data.data(), random_data.size()),
                SafeFD::Error::kNotInitialized);
}

TEST_F(SafeFDTest, Write_VerifyTruncate) {
  std::string random_data = GetRandomSuffix();
  ASSERT_TRUE(WriteFile(random_data));

  {
    SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
    ASSERT_TRUE(file.first.is_valid());

    EXPECT_STR_EQ(file.first.Write("", 0), SafeFD::Error::kNoError);
  }

  ExpectFileContains("");
}

TEST_F(SafeFDTest, Write_Failure) {
  std::string random_data = GetRandomSuffix();
  EXPECT_STR_EQ(root_.Write("", 1), SafeFD::Error::kIOError);
}

TEST_F(SafeFDTest, ReadContents_Success) {
  std::string random_data = GetRandomSuffix();
  ASSERT_TRUE(WriteFile(random_data));

  SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
  ASSERT_TRUE(file.first.is_valid());

  auto result = file.first.ReadContents();
  EXPECT_STR_EQ(result.second, SafeFD::Error::kNoError);
  ASSERT_EQ(random_data.size(), result.first.size());
  EXPECT_EQ(memcmp(random_data.data(), result.first.data(), random_data.size()),
            0);
}

TEST_F(SafeFDTest, ReadContents_ExceededMaximum) {
  std::string random_data = GetRandomSuffix();
  ASSERT_TRUE(WriteFile(random_data));

  SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
  ASSERT_TRUE(file.first.is_valid());

  ASSERT_LT(1, random_data.size());
  auto result = file.first.ReadContents(1);
  EXPECT_STR_EQ(result.second, SafeFD::Error::kExceededMaximum);
}

TEST_F(SafeFDTest, ReadContents_NotInitialized) {
  SafeFD invalid;
  ASSERT_FALSE(invalid.is_valid());

  auto result = invalid.ReadContents();
  EXPECT_STR_EQ(result.second, SafeFD::Error::kNotInitialized);
}

TEST_F(SafeFDTest, Read_Success) {
  std::string random_data = GetRandomSuffix();
  ASSERT_TRUE(WriteFile(random_data));

  SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
  ASSERT_TRUE(file.first.is_valid());

  std::vector<char> buffer(random_data.size(), '\0');
  ASSERT_EQ(file.first.Read(buffer.data(), buffer.size()),
            SafeFD::Error::kNoError);
  EXPECT_EQ(memcmp(random_data.data(), buffer.data(), random_data.size()), 0);
}

TEST_F(SafeFDTest, Read_NotInitialized) {
  SafeFD invalid;
  ASSERT_FALSE(invalid.is_valid());

  char to_read;
  EXPECT_STR_EQ(invalid.Read(&to_read, 1), SafeFD::Error::kNotInitialized);
}

TEST_F(SafeFDTest, Read_IOError) {
  std::string random_data = GetRandomSuffix();
  ASSERT_TRUE(WriteFile(random_data));

  SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
  ASSERT_TRUE(file.first.is_valid());

  std::vector<char> buffer(random_data.size() * 2, '\0');
  ASSERT_EQ(file.first.Read(buffer.data(), buffer.size()),
            SafeFD::Error::kIOError);
}

TEST_F(SafeFDTest, OpenExistingFile_Success) {
  std::string data = GetRandomSuffix();
  ASSERT_TRUE(WriteFile(data));
  {
    SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
    ASSERT_TRUE(file.first.is_valid());
  }
  ExpectFileContains(data);
}

TEST_F(SafeFDTest, OpenExistingFile_NotInitialized) {
  SafeFD::SafeFDResult file = SafeFD().OpenExistingFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kNotInitialized);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingFile_DoesNotExist) {
  SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kDoesNotExist);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingFile_IOError) {
  ASSERT_TRUE(WriteFile(""));
  EXPECT_EQ(chmod(file_path_.value().c_str(), 0000), 0) << strerror(errno);

  SafeFD::SafeFDResult file = root_.OpenExistingFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kIOError);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingFile_SymlinkDetected) {
  ASSERT_TRUE(SetupSymlinks());
  ASSERT_TRUE(WriteFile(""));
  SafeFD::SafeFDResult file = root_.OpenExistingFile(symlink_file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kSymlinkDetected);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingFile_WrongType) {
  ASSERT_TRUE(SetupSymlinks());
  ASSERT_TRUE(WriteFile(""));
  SafeFD::SafeFDResult file =
      root_.OpenExistingFile(symlink_dir_path_.Append(kFileName));
  EXPECT_STR_EQ(file.second, SafeFD::Error::kWrongType);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingDir_Success) {
  SafeFD::SafeFDResult dir = root_.OpenExistingDir(temp_dir_.GetPath());
  EXPECT_STR_EQ(dir.second, SafeFD::Error::kNoError);
  ASSERT_TRUE(dir.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingDir_NotInitialized) {
  SafeFD::SafeFDResult dir = SafeFD().OpenExistingDir(temp_dir_.GetPath());
  EXPECT_STR_EQ(dir.second, SafeFD::Error::kNotInitialized);
  ASSERT_FALSE(dir.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingDir_DoesNotExist) {
  SafeFD::SafeFDResult dir = root_.OpenExistingDir(sub_dir_path_);
  EXPECT_STR_EQ(dir.second, SafeFD::Error::kDoesNotExist);
  ASSERT_FALSE(dir.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingDir_IOError) {
  ASSERT_TRUE(WriteFile(""));
  EXPECT_EQ(chmod(sub_dir_path_.value().c_str(), 0000), 0) << strerror(errno);

  SafeFD::SafeFDResult dir = root_.OpenExistingDir(sub_dir_path_);
  EXPECT_STR_EQ(dir.second, SafeFD::Error::kIOError);
  ASSERT_FALSE(dir.first.is_valid());
}

TEST_F(SafeFDTest, OpenExistingDir_WrongType) {
  ASSERT_TRUE(SetupSymlinks());
  SafeFD::SafeFDResult dir = root_.OpenExistingDir(symlink_dir_path_);
  EXPECT_STR_EQ(dir.second, SafeFD::Error::kWrongType);
  ASSERT_FALSE(dir.first.is_valid());
}

TEST_F(SafeFDTest, MakeFile_DoesNotExistSuccess) {
  {
    SafeFD::SafeFDResult file = root_.MakeFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
    ASSERT_TRUE(file.first.is_valid());
  }

  ExpectPermissions(file_path_, SafeFD::kDefaultFilePermissions);
}

TEST_F(SafeFDTest, MakeFile_LeadingSelfDirSuccess) {
  ASSERT_TRUE(SetupSubdir());

  SafeFD::Error err;
  SafeFD dir;
  std::tie(dir, err) = root_.OpenExistingDir(sub_dir_path_);
  EXPECT_STR_EQ(err, SafeFD::Error::kNoError);

  {
    SafeFD file;
    std::tie(file, err) = dir.MakeFile(file_path_.BaseName());
    EXPECT_STR_EQ(err, SafeFD::Error::kNoError);
    ASSERT_TRUE(file.is_valid());
  }

  ExpectPermissions(file_path_, SafeFD::kDefaultFilePermissions);
}

TEST_F(SafeFDTest, MakeFile_ExistsSuccess) {
  std::string data = GetRandomSuffix();
  ASSERT_TRUE(WriteFile(data));
  {
    SafeFD::SafeFDResult file = root_.MakeFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kNoError);
    ASSERT_TRUE(file.first.is_valid());
  }
  ExpectPermissions(file_path_, SafeFD::kDefaultFilePermissions);
  ExpectFileContains(data);
}

TEST_F(SafeFDTest, MakeFile_IOError) {
  ASSERT_TRUE(SetupSubdir());
  ASSERT_EQ(mkfifo(file_path_.value().c_str(), 0), 0);
  SafeFD::SafeFDResult file = root_.MakeFile(file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kIOError);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, MakeFile_SymlinkDetected) {
  ASSERT_TRUE(SetupSymlinks());
  SafeFD::SafeFDResult file = root_.MakeFile(symlink_file_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kSymlinkDetected);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, MakeFile_WrongType) {
  ASSERT_TRUE(SetupSubdir());
  SafeFD::SafeFDResult file = root_.MakeFile(sub_dir_path_);
  EXPECT_STR_EQ(file.second, SafeFD::Error::kWrongType);
  ASSERT_FALSE(file.first.is_valid());
}

TEST_F(SafeFDTest, MakeFile_WrongGID) {
  ASSERT_TRUE(WriteFile(""));
  EXPECT_EQ(chown(file_path_.value().c_str(), getuid(), 0), 0)
      << strerror(errno);
  {
    SafeFD::SafeFDResult file = root_.MakeFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kWrongGID);
    ASSERT_FALSE(file.first.is_valid());
  }
}

TEST_F(SafeFDTest, MakeFile_WrongPermissions) {
  ASSERT_TRUE(WriteFile(""));
  EXPECT_EQ(chmod(file_path_.value().c_str(), 0777), 0) << strerror(errno);
  {
    SafeFD::SafeFDResult file = root_.MakeFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kWrongPermissions);
    ASSERT_FALSE(file.first.is_valid());
  }
  EXPECT_EQ(chmod(file_path_.value().c_str(), SafeFD::kDefaultFilePermissions),
            0)
      << strerror(errno);

  EXPECT_EQ(chmod(sub_dir_path_.value().c_str(), 0777), 0) << strerror(errno);
  {
    SafeFD::SafeFDResult file = root_.MakeFile(file_path_);
    EXPECT_STR_EQ(file.second, SafeFD::Error::kWrongPermissions);
    ASSERT_FALSE(file.first.is_valid());
  }
}

TEST_F(SafeFDTest, MakeDir_DoesNotExistSuccess) {
  {
    SafeFD::SafeFDResult dir = root_.MakeDir(sub_dir_path_);
    EXPECT_STR_EQ(dir.second, SafeFD::Error::kNoError);
    ASSERT_TRUE(dir.first.is_valid());
  }

  ExpectPermissions(sub_dir_path_, SafeFD::kDefaultDirPermissions);
}

TEST_F(SafeFDTest, MakeFile_SingleComponentSuccess) {
  ASSERT_TRUE(SetupSubdir());

  SafeFD::Error err;
  SafeFD dir;
  std::tie(dir, err) = root_.OpenExistingDir(temp_dir_.GetPath());
  EXPECT_STR_EQ(err, SafeFD::Error::kNoError);

  {
    SafeFD subdir;
    std::tie(subdir, err) = dir.MakeDir(base::FilePath(kSubdirName));
    EXPECT_STR_EQ(err, SafeFD::Error::kNoError);
    ASSERT_TRUE(subdir.is_valid());
  }

  ExpectPermissions(sub_dir_path_, SafeFD::kDefaultDirPermissions);
}

TEST_F(SafeFDTest, MakeDir_ExistsSuccess) {
  ASSERT_TRUE(SetupSubdir());
  {
    SafeFD::SafeFDResult dir = root_.MakeDir(sub_dir_path_);
    EXPECT_STR_EQ(dir.second, SafeFD::Error::kNoError);
    ASSERT_TRUE(dir.first.is_valid());
  }

  ExpectPermissions(sub_dir_path_, SafeFD::kDefaultDirPermissions);
}

TEST_F(SafeFDTest, MakeDir_WrongType) {
  ASSERT_TRUE(SetupSymlinks());
  SafeFD::SafeFDResult dir = root_.MakeDir(symlink_dir_path_);
  EXPECT_STR_EQ(dir.second, SafeFD::Error::kWrongType);
  ASSERT_FALSE(dir.first.is_valid());
}

TEST_F(SafeFDTest, MakeDir_WrongGID) {
  ASSERT_TRUE(SetupSubdir());
  EXPECT_EQ(chown(sub_dir_path_.value().c_str(), getuid(), 0), 0)
      << strerror(errno);
  {
    SafeFD::SafeFDResult dir = root_.MakeDir(sub_dir_path_);
    EXPECT_STR_EQ(dir.second, SafeFD::Error::kWrongGID);
    ASSERT_FALSE(dir.first.is_valid());
  }
}

TEST_F(SafeFDTest, MakeDir_WrongPermissions) {
  ASSERT_TRUE(SetupSubdir());
  EXPECT_EQ(chmod(sub_dir_path_.value().c_str(), 0777), 0) << strerror(errno);

  SafeFD::SafeFDResult dir = root_.MakeDir(sub_dir_path_);
  EXPECT_STR_EQ(dir.second, SafeFD::Error::kWrongPermissions);
  ASSERT_FALSE(dir.first.is_valid());
}

}  // namespace brillo
