// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/platform.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <linux/fs.h>

#include <fcntl.h>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using base::FilePath;

namespace cryptohome {

class PlatformTest : public ::testing::Test {
 public:
  virtual ~PlatformTest() {}
 protected:
  std::string GetRandomSuffix() {
    return platform_.GetRandomSuffix();
  }
  FilePath GetTempName() {
    FilePath temp_directory;
    EXPECT_TRUE(base::GetTempDir(&temp_directory));
    return temp_directory.Append(GetRandomSuffix());
  }

  Platform platform_;
};

TEST_F(PlatformTest, WriteFileCanBeReadBack) {
  const FilePath filename(GetTempName());
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
  const FilePath filename(GetTempName());
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
  const FilePath dirname(GetTempName());
  const FilePath subdirname(dirname.Append(GetRandomSuffix()));
  const FilePath filename(subdirname.Append(GetRandomSuffix()));
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
  const FilePath filename(GetTempName());
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
  const FilePath filename(GetTempName());
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
  const FilePath filename(GetTempName());
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
  const FilePath dirname(GetTempName());
  const FilePath subdirname(dirname.Append(GetRandomSuffix()));
  const FilePath filename(subdirname.Append(GetRandomSuffix()));
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
  const FilePath filename(GetTempName());
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
  const FilePath filename(GetTempName());
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
  const FilePath filename(GetTempName());
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
  const FilePath dirname(GetTempName());
  const FilePath subdirname(dirname.Append(GetRandomSuffix()));
  const FilePath filename(subdirname.Append(GetRandomSuffix()));
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
  const FilePath filename(GetTempName());
  EXPECT_TRUE(platform_.TouchFileDurable(filename));
  int64_t size = -1;
  EXPECT_TRUE(platform_.GetFileSize(filename, &size));
  EXPECT_EQ(0, size);
  platform_.DeleteFile(filename, false /* recursive */);
}

TEST_F(PlatformTest, TouchFileDurableSets0666) {
  const mode_t mask = 0000;
  const mode_t mode = 0666;
  const FilePath filename(GetTempName());
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
  const FilePath filename(GetTempName());
  const mode_t old_mask = platform_.SetMask(mask);
  EXPECT_TRUE(platform_.TouchFileDurable(filename));
  mode_t file_mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.SetMask(old_mask);
}

TEST_F(PlatformTest, DataSyncFileHasSaneReturnCodes) {
  const FilePath filename(GetTempName());
  const FilePath dirname(GetTempName());
  platform_.CreateDirectory(dirname);
  EXPECT_FALSE(platform_.DataSyncFile(dirname));
  EXPECT_FALSE(platform_.DataSyncFile(filename));
  EXPECT_TRUE(platform_.WriteStringToFile(filename, "bla"));
  EXPECT_TRUE(platform_.DataSyncFile(filename));
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.DeleteFile(dirname, true /* recursive */);
}

TEST_F(PlatformTest, SyncFileHasSaneReturnCodes) {
  const FilePath filename(GetTempName());
  const FilePath dirname(GetTempName());
  platform_.CreateDirectory(dirname);
  EXPECT_FALSE(platform_.SyncFile(dirname));
  EXPECT_FALSE(platform_.SyncFile(filename));
  EXPECT_TRUE(platform_.WriteStringToFile(filename, "bla"));
  EXPECT_TRUE(platform_.SyncFile(filename));
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.DeleteFile(dirname, true /* recursive */);
}

TEST_F(PlatformTest, SyncDirectoryHasSaneReturnCodes) {
  const FilePath filename(GetTempName());
  const FilePath dirname(GetTempName());
  platform_.WriteStringToFile(filename, "bla");
  EXPECT_FALSE(platform_.SyncDirectory(filename));
  EXPECT_FALSE(platform_.SyncDirectory(dirname));
  EXPECT_TRUE(platform_.CreateDirectory(dirname));
  EXPECT_TRUE(platform_.SyncDirectory(dirname));
  platform_.DeleteFile(filename, false /* recursive */);
  platform_.DeleteFile(dirname, true /* recursive */);
}

TEST_F(PlatformTest, HasExtendedFileAttribute) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));
  const std::string name("user.foo");
  const std::string value("bar");

  ASSERT_EQ(0, setxattr(filename.value().c_str(), name.c_str(), value.c_str(),
                        value.length(), 0));

  EXPECT_TRUE(platform_.HasExtendedFileAttribute(filename, name));

  EXPECT_FALSE(platform_.HasExtendedFileAttribute(
        FilePath("file_not_exist"), name));
  EXPECT_FALSE(platform_.HasExtendedFileAttribute(
        filename, "user.name_not_exist"));
}

TEST_F(PlatformTest, ListExtendedFileAttribute) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));
  const std::string name("user.foo");
  const std::string value("bar");
  const std::string name2("user.foo2");
  const std::string value2("bar2");

  ASSERT_EQ(0,
            setxattr(filename.value().c_str(),
                     name.c_str(),
                     value.c_str(),
                     value.length(),
                     0));
  ASSERT_EQ(0,
            setxattr(filename.value().c_str(),
                     name2.c_str(),
                     value2.c_str(),
                     value2.length(),
                     0));

  std::vector<std::string> attrs;

  EXPECT_TRUE(platform_.ListExtendedFileAttributes(filename, &attrs));
  EXPECT_THAT(attrs, testing::UnorderedElementsAre(name, name2));

  attrs.clear();
  EXPECT_FALSE(
      platform_.ListExtendedFileAttributes(FilePath("file_not_exist"), &attrs));
  EXPECT_TRUE(attrs.empty());
}

TEST_F(PlatformTest, GetExtendedAttributeAsString) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));
  const std::string name("user.foo");
  const std::string value("bar");

  ASSERT_EQ(0,
            setxattr(filename.value().c_str(),
                     name.c_str(),
                     value.c_str(),
                     value.length(),
                     0));

  std::string got;
  EXPECT_TRUE(platform_.GetExtendedFileAttributeAsString(filename, name, &got));
  EXPECT_EQ(value, got);

  EXPECT_FALSE(platform_.GetExtendedFileAttributeAsString(
      FilePath("file_not_exist"), name, &got));
  EXPECT_FALSE(platform_.GetExtendedFileAttributeAsString(
      filename, "user.name_not_exist", &got));
}

TEST_F(PlatformTest, GetExtendedAttribute) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));
  const std::string name("user.foo");
  const int value = 42;

  ASSERT_EQ(
      0,
      setxattr(
          filename.value().c_str(), name.c_str(), &value, sizeof(value), 0));

  int got;
  EXPECT_TRUE(platform_.GetExtendedFileAttribute(
      filename, name, reinterpret_cast<char*>(&got), sizeof(got)));
  EXPECT_EQ(value, got);

  EXPECT_FALSE(platform_.GetExtendedFileAttribute(FilePath("file_not_exist"),
                                                  name,
                                                  reinterpret_cast<char*>(&got),
                                                  sizeof(got)));
  EXPECT_FALSE(platform_.GetExtendedFileAttribute(filename,
                                                  "user.name_not_exist",
                                                  reinterpret_cast<char*>(&got),
                                                  sizeof(got)));
  EXPECT_FALSE(platform_.GetExtendedFileAttribute(
      filename, name, reinterpret_cast<char*>(&got), sizeof(got) - 1));
  EXPECT_FALSE(platform_.GetExtendedFileAttribute(
      filename, name, reinterpret_cast<char*>(&got), sizeof(got) + 1));
}

TEST_F(PlatformTest, SetExtendedAttribute) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));
  const std::string name("user.foo");
  std::string value("bar");
  EXPECT_TRUE(platform_.SetExtendedFileAttribute(
      filename, name, value.c_str(), value.length()));

  std::vector<char> got(value.length());
  EXPECT_EQ(
      value.length(),
      getxattr(
          filename.value().c_str(), name.c_str(), got.data(), value.length()));

  EXPECT_EQ(value, std::string(got.data(), got.size()));

  EXPECT_FALSE(platform_.SetExtendedFileAttribute(
      FilePath("file_not_exist"), name, value.c_str(), sizeof(value)));
}

TEST_F(PlatformTest, GetExtFileAttributes) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));

  int fd;
  ASSERT_GT(fd = HANDLE_EINTR(open(filename.value().c_str(), O_RDONLY)), 0);
  int flags = FS_UNRM_FL | FS_NODUMP_FL;
  ASSERT_GE(ioctl(fd, FS_IOC_SETFLAGS, &flags), 0);

  int got;
  EXPECT_TRUE(platform_.GetExtFileAttributes(filename, &got));
  EXPECT_EQ(flags, got);
  close(fd);
}

TEST_F(PlatformTest, SetExtFileAttributes) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));

  int flags = FS_UNRM_FL | FS_NODUMP_FL;
  EXPECT_TRUE(platform_.SetExtFileAttributes(filename, flags));

  int fd;
  ASSERT_GT(fd = HANDLE_EINTR(open(filename.value().c_str(), O_RDONLY)), 0);
  int new_flags;
  ASSERT_GE(ioctl(fd, FS_IOC_GETFLAGS, &new_flags), 0);

  EXPECT_EQ(flags, new_flags);
  close(fd);
}

TEST_F(PlatformTest, HasNoDumpFileAttribute) {
  const FilePath filename(GetTempName());
  const std::string content("blablabla");
  ASSERT_TRUE(platform_.WriteStringToFile(filename, content));

  EXPECT_FALSE(platform_.HasNoDumpFileAttribute(filename));

  int fd;
  ASSERT_GT(fd = open(filename.value().c_str(), O_RDONLY), 0);
  int flags = FS_UNRM_FL | FS_NODUMP_FL;
  ASSERT_GE(ioctl(fd, FS_IOC_SETFLAGS, &flags), 0);

  EXPECT_TRUE(platform_.HasNoDumpFileAttribute(filename));
  close(fd);
}

TEST_F(PlatformTest, DecodeProcInfoLineGood) {
  std::string mount_info_contents;

  mount_info_contents.append("73 24 179:1 /beg/uid1/mount/user ");
  mount_info_contents.append("/home/user/uid1 rw,nodev,relatime - ext4 ");
  mount_info_contents.append("/dev/mmcblk0p1 rw,commit=600,data=ordered");

  std::vector<std::string> args;
  size_t fs_idx;

  EXPECT_TRUE(platform_.DecodeProcInfoLine(
        mount_info_contents, &args, &fs_idx));
  EXPECT_EQ(fs_idx, 7);
}

TEST_F(PlatformTest, DecodeProcInfoLineCorruptedMountInfo) {
  std::string mount_info_contents;

  mount_info_contents.append("73 24 179:1 /beg/uid1/mount/user ");
  mount_info_contents.append("/home/user/uid1 rw,nodev,relatime hypen ext4 ");
  mount_info_contents.append("/dev/mmcblk0p1 rw,commit=600,data=ordered");

  std::vector<std::string> args;
  size_t fs_idx;

  EXPECT_FALSE(platform_.DecodeProcInfoLine(
        mount_info_contents, &args, &fs_idx));
}

TEST_F(PlatformTest, DecodeProcInfoLineIncompleteMountInfo) {
  std::string mount_info_contents;

  mount_info_contents.append("73 24 179:1 /beg/uid1/mount/user ");

  std::vector<std::string> args;
  size_t fs_idx;

  EXPECT_FALSE(platform_.DecodeProcInfoLine(
        mount_info_contents, &args, &fs_idx));
}

TEST_F(PlatformTest, GetMountsBySourcePrefixExt4) {
  base::FilePath mount_info;
  FILE *fp;
  std::string filesystem, device_in, device_out, mount_info_contents;

  mount_info_contents.append("73 24 179:1 /beg/uid1/mount/user ");
  mount_info_contents.append("/home/user/uid1 rw,nodev,relatime - ext4 ");
  mount_info_contents.append("/dev/mmcblk0p1 rw,commit=600,data=ordered");

  fp = base::CreateAndOpenTemporaryFile(&mount_info);
  ASSERT_TRUE(fp != NULL);
  EXPECT_EQ(fwrite(mount_info_contents.c_str(),
            mount_info_contents.length(), 1, fp), 1);
  EXPECT_EQ(fclose(fp), 0);

  platform_.set_mount_info_path(mount_info);

  /* Fails if item is missing. */
  std::multimap<const FilePath, const FilePath> mounts;
  EXPECT_FALSE(platform_.GetMountsBySourcePrefix(FilePath("monkey"), &mounts));

  /* Works normally. */
  mounts.clear();
  EXPECT_TRUE(platform_.GetMountsBySourcePrefix(FilePath("/beg"), &mounts));
  EXPECT_EQ(mounts.size(), 1);
  auto it = mounts.begin();
  EXPECT_EQ(it->first.value(), "/beg/uid1/mount/user");
  EXPECT_EQ(it->second.value(), "/home/user/uid1");

  /* Clean up. */
  EXPECT_TRUE(base::DeleteFile(mount_info, false));
}

TEST_F(PlatformTest, GetMountsBySourcePrefixECryptFs) {
  base::FilePath mount_info;
  FILE *fp;
  std::string filesystem, device_in, device_out, mount_info_contents;

  mount_info_contents.append("84 24 0:29 /user /home/user/uid2 ");
  mount_info_contents.append("rw,nosuid,nodev,noexec,relatime - ecryptfs ");
  mount_info_contents.append("/beg/uid2/vault rw,ecryp...");

  fp = base::CreateAndOpenTemporaryFile(&mount_info);
  ASSERT_TRUE(fp != NULL);
  EXPECT_EQ(fwrite(mount_info_contents.c_str(),
            mount_info_contents.length(), 1, fp), 1);
  EXPECT_EQ(fclose(fp), 0);

  platform_.set_mount_info_path(mount_info);

  /* Fails if item is missing. */
  std::multimap<const FilePath, const FilePath> mounts;
  EXPECT_FALSE(platform_.GetMountsBySourcePrefix(FilePath("monkey"), &mounts));

  /* Works normally. */
  mounts.clear();
  EXPECT_TRUE(platform_.GetMountsBySourcePrefix(FilePath("/beg"), &mounts));
  EXPECT_EQ(mounts.size(), 1);
  auto it = mounts.begin();
  EXPECT_EQ(it->first.value(), "/beg/uid2/vault");
  EXPECT_EQ(it->second.value(), "/home/user/uid2");

  /* Clean up. */
  EXPECT_TRUE(base::DeleteFile(mount_info, false));
}

TEST_F(PlatformTest, CreateSymbolicLink) {
  const base::FilePath link(GetTempName());
  const base::FilePath target(GetTempName());
  const base::FilePath existing_file(GetTempName());
  ASSERT_TRUE(platform_.TouchFileDurable(existing_file));
  EXPECT_TRUE(platform_.CreateSymbolicLink(link, target));
  EXPECT_FALSE(platform_.CreateSymbolicLink(existing_file, target));
  base::FilePath read_target;
  ASSERT_TRUE(base::ReadSymbolicLink(link, &read_target));
  EXPECT_EQ(target.value(), read_target.value());
}

TEST_F(PlatformTest, ReadLink) {
  const base::FilePath valid_link(GetTempName());
  const base::FilePath not_link(GetTempName());
  const base::FilePath target(GetTempName());
  ASSERT_TRUE(base::CreateSymbolicLink(target, valid_link));
  ASSERT_TRUE(platform_.TouchFileDurable(not_link));
  base::FilePath read_target;
  EXPECT_TRUE(platform_.ReadLink(valid_link, &read_target));
  EXPECT_EQ(target.value(), read_target.value());
  EXPECT_FALSE(platform_.ReadLink(not_link, &read_target));
}

TEST_F(PlatformTest, SetFileTimes) {
  struct timespec atime1 = {123, 45};
  struct timespec mtime1 = {234, 56};
  struct timespec atime2 = {345, 67};
  struct timespec mtime2 = {456, 78};
  const base::FilePath regular_file(GetTempName());
  const base::FilePath link(GetTempName());
  ASSERT_TRUE(platform_.TouchFileDurable(regular_file));
  ASSERT_TRUE(platform_.CreateSymbolicLink(link, regular_file));

  EXPECT_TRUE(platform_.SetFileTimes(regular_file, atime1, mtime1, true));
  struct stat stat;
  ASSERT_TRUE(platform_.Stat(regular_file, &stat));
  EXPECT_EQ(atime1.tv_sec, stat.st_atim.tv_sec);
  EXPECT_EQ(atime1.tv_nsec, stat.st_atim.tv_nsec);
  EXPECT_EQ(mtime1.tv_sec, stat.st_mtim.tv_sec);
  EXPECT_EQ(mtime1.tv_nsec, stat.st_mtim.tv_nsec);

  EXPECT_TRUE(platform_.SetFileTimes(link, atime2, mtime2, true));
  ASSERT_TRUE(platform_.Stat(regular_file, &stat));
  EXPECT_EQ(atime2.tv_sec, stat.st_atim.tv_sec);
  EXPECT_EQ(atime2.tv_nsec, stat.st_atim.tv_nsec);
  EXPECT_EQ(mtime2.tv_sec, stat.st_mtim.tv_sec);
  EXPECT_EQ(mtime2.tv_nsec, stat.st_mtim.tv_nsec);
  ASSERT_TRUE(platform_.Stat(link, &stat));
  EXPECT_NE(atime2.tv_sec, stat.st_atim.tv_sec);
  EXPECT_NE(atime2.tv_nsec, stat.st_atim.tv_nsec);
  EXPECT_NE(mtime2.tv_sec, stat.st_mtim.tv_sec);
  EXPECT_NE(mtime2.tv_nsec, stat.st_mtim.tv_nsec);
}

}  // namespace cryptohome
