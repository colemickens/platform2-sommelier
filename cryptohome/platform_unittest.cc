// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/platform.h"

#include <sys/types.h>

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <gtest/gtest.h>

namespace cryptohome {

class PlatformTest : public ::testing::Test {
 public:
  virtual ~PlatformTest() {}
 protected:
  std::string GetRandomSuffix() {
    return platform_.GetRandomSuffix();
  }
  std::string GetTempName() {
    base::FilePath temp_directory;
    EXPECT_TRUE(base::GetTempDir(&temp_directory));
    return temp_directory.Append(GetRandomSuffix()).value();
  }

  Platform platform_;
};

TEST_F(PlatformTest, WriteFileCanBeReadBack) {
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  EXPECT_TRUE(platform_.WriteStringToFile(filename, content));
  std::string output;
  EXPECT_TRUE(platform_.ReadFileToString(filename, &output));
  EXPECT_EQ(content, output);
  platform_.DeleteFile(filename, false /* recursive */);
}

TEST_F(PlatformTest, WriteFileSets0666) {
  const mode_t mask = 0000;
  const mode_t mode = 0666;
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(platform_.WriteStringToFile(filename, content));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.SetMask(old_mask);
  platform_.DeleteFile(filename, false /* recursive */);
}

TEST_F(PlatformTest, WriteFileCreatesMissingParentDirectoriesWith0700) {
  const mode_t mask = 0000;
  const mode_t mode = 0700;
  const std::string dirname(GetTempName());
  const std::string subdirname(
      base::FilePath(dirname).Append(GetRandomSuffix()).value());
  const std::string filename(
      base::FilePath(subdirname).Append(GetRandomSuffix()).value());
  const std::string content("blablabla");
  EXPECT_TRUE(platform_.WriteStringToFile(filename, content));
  mode_t dir_mode = 0;
  mode_t subdir_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(dirname, &dir_mode));
  EXPECT_TRUE(platform_.GetPermissions(subdirname, &subdir_mode));
  EXPECT_EQ(mode & ~mask, dir_mode & 0777);
  EXPECT_EQ(mode & ~mask, subdir_mode & 0777);
  const mode_t old_mask = platform_.SetMask(mask);
  platform_.DeleteFile(dirname, true /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, WriteStringToFileAtomicCanBeReadBack) {
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  EXPECT_TRUE(platform_.WriteStringToFileAtomic(filename, content, 0644));
  std::string output;
  EXPECT_TRUE(platform_.ReadFileToString(filename, &output));
  EXPECT_EQ(content, output);
  platform_.DeleteFile(filename, false /* recursive */);
}

TEST_F(PlatformTest, WriteStringToFileAtomicHonorsMode) {
  const mode_t mask = 0000;
  const mode_t mode = 0616;
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(platform_.WriteStringToFileAtomic(filename, content, mode));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, WriteStringToFileAtomicHonorsUmask) {
  const mode_t mask = 0073;
  const mode_t mode = 0777;
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(platform_.WriteStringToFileAtomic(filename, content, mode));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest,
       WriteStringToFileAtomicCreatesMissingParentDirectoriesWith0700) {
  const mode_t mask = 0000;
  const mode_t mode = 0700;
  const std::string dirname(GetTempName());
  const std::string subdirname(
      base::FilePath(dirname).Append(GetRandomSuffix()).value());
  const std::string filename(
      base::FilePath(subdirname).Append(GetRandomSuffix()).value());
  const std::string content("blablabla");
  EXPECT_TRUE(platform_.WriteStringToFileAtomic(filename, content, 0777));
  mode_t dir_mode = 0;
  mode_t subdir_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(dirname, &dir_mode));
  EXPECT_TRUE(platform_.GetPermissions(subdirname, &subdir_mode));
  EXPECT_EQ(mode & ~mask, dir_mode & 0777);
  EXPECT_EQ(mode & ~mask, subdir_mode & 0777);
  const mode_t old_mask = platform_.SetMask(mask);
  platform_.DeleteFile(dirname, true /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, WriteStringToFileAtomicDurableCanBeReadBack) {
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  EXPECT_TRUE(
      platform_.WriteStringToFileAtomicDurable(filename, content, 0644));
  std::string output;
  EXPECT_TRUE(platform_.ReadFileToString(filename, &output));
  EXPECT_EQ(content, output);
  platform_.DeleteFile(filename, false /* recursive */);
}

TEST_F(PlatformTest, WriteStringToFileAtomicDurableHonorsMode) {
  const mode_t mask = 0000;
  const mode_t mode = 0616;
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(
      platform_.WriteStringToFileAtomicDurable(filename, content, mode));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, WriteStringToFileAtomicDurableHonorsUmask) {
  const mode_t mask = 0073;
  const mode_t mode = 0777;
  const std::string filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(
      platform_.WriteStringToFileAtomicDurable(filename, content, mode));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest,
       WriteStringToFileAtomicDurableCreatesMissingParentDirectoriesWith0700) {
  const mode_t mask = 0000;
  const mode_t mode = 0700;
  const std::string dirname(GetTempName());
  const std::string subdirname(
      base::FilePath(dirname).Append(GetRandomSuffix()).value());
  const std::string filename(
      base::FilePath(subdirname).Append(GetRandomSuffix()).value());
  const std::string content("blablabla");
  EXPECT_TRUE(platform_.WriteStringToFileAtomicDurable(
      filename, content, 0777));
  mode_t dir_mode = 0;
  mode_t subdir_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(dirname, &dir_mode));
  EXPECT_TRUE(platform_.GetPermissions(subdirname, &subdir_mode));
  EXPECT_EQ(mode & ~mask, dir_mode & 0777);
  EXPECT_EQ(mode & ~mask, subdir_mode & 0777);
  const mode_t old_mask = platform_.SetMask(mask);
  platform_.DeleteFile(dirname, true /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, TouchFileDurable) {
  const std::string filename(GetTempName());
  EXPECT_TRUE(platform_.TouchFileDurable(filename));
  int64_t size = -1;
  EXPECT_TRUE(platform_.GetFileSize(filename, &size));
  EXPECT_EQ(0, size);
  platform_.DeleteFile(filename, false /* recursive */);
}

TEST_F(PlatformTest, TouchFileDurableSets0666) {
  const mode_t mask = 0000;
  const mode_t mode = 0666;
  const std::string filename(GetTempName());
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(platform_.TouchFileDurable(filename));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, TouchFileDurableHonorsUmask) {
  const mode_t mask = 0066;
  const mode_t mode = 0640;
  const std::string filename(GetTempName());
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(platform_.TouchFileDurable(filename));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, DataSyncFileHasSaneReturnCodes) {
  const std::string filename(GetTempName());
  const std::string dirname(GetTempName());
  platform_.CreateDirectory(dirname);
  EXPECT_FALSE(platform_.DataSyncFile(dirname));
  EXPECT_FALSE(platform_.DataSyncFile(filename));
  EXPECT_TRUE(platform_.WriteStringToFile(filename, "bla"));
  EXPECT_TRUE(platform_.DataSyncFile(filename));
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.DeleteFile(dirname, true /* recursive */);
}

TEST_F(PlatformTest, SyncDirectoryHasSaneReturnCodes) {
  const std::string filename(GetTempName());
  const std::string dirname(GetTempName());
  platform_.WriteStringToFile(filename, "bla");
  EXPECT_FALSE(platform_.SyncDirectory(filename));
  EXPECT_FALSE(platform_.SyncDirectory(dirname));
  EXPECT_TRUE(platform_.CreateDirectory(dirname));
  EXPECT_TRUE(platform_.SyncDirectory(dirname));
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.DeleteFile(dirname, true /* recursive */);
}

}  // namespace cryptohome
