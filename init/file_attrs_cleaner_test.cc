// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/file_attrs_cleaner.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <linux/fs.h>

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

using file_attrs_cleaner::AttributeCheckStatus;
using file_attrs_cleaner::CheckFileAttributes;
using file_attrs_cleaner::ImmutableAllowed;
using file_attrs_cleaner::RemoveURLExtendedAttributes;
using file_attrs_cleaner::ScanDir;

namespace {

// Helper to create a test file.
bool CreateFile(const base::FilePath& file_path, base::StringPiece content) {
  if (!base::CreateDirectory(file_path.DirName()))
    return false;
  return base::WriteFile(file_path, content.data(), content.size()) ==
         content.size();
}

}  // namespace

namespace {

class CheckFileAttributesTest : public ::testing::Test {
  void SetUp() {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_dir_ = scoped_temp_dir_.GetPath();
  }

 protected:
  // Setting file attributes (like immutable) requires privileges.
  // If we don't have that, we can't validate these tests :/.
  bool CanSetFileAttributes() {
    const base::FilePath path = test_dir_.Append(".attrs.test");

    base::ScopedFD fd(open(path.value().c_str(),
                           O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0600));
    if (!fd.is_valid())
      abort();

    long flags;  // NOLINT(runtime/int)
    if (ioctl(fd.get(), FS_IOC_GETFLAGS, &flags) != 0)
      abort();

    flags |= FS_IMMUTABLE_FL;
    if (ioctl(fd.get(), FS_IOC_SETFLAGS, &flags) != 0) {
      PLOG(WARNING) << "Unable to test immutable bit behavior";
      if (errno != EPERM)
        abort();
      return false;
    }

    return true;
  }

  base::FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;
};

}  // namespace

TEST_F(CheckFileAttributesTest, BadFd) {
  const base::FilePath path = test_dir_.Append("asdf");
  EXPECT_EQ(AttributeCheckStatus::ERROR, CheckFileAttributes(path, false, -1));
  EXPECT_EQ(AttributeCheckStatus::ERROR, CheckFileAttributes(path, true, -1));
  EXPECT_EQ(AttributeCheckStatus::ERROR, CheckFileAttributes(path, true, 1000));
  EXPECT_EQ(AttributeCheckStatus::ERROR,
            CheckFileAttributes(path, false, 1000));
}

// Accept paths w/out the immutable bit set.
TEST_F(CheckFileAttributesTest, NormalPaths) {
  const base::FilePath path = test_dir_.Append("file");
  ASSERT_TRUE(CreateFile(path, ""));

  base::ScopedFD fd(open(path.value().c_str(), O_RDONLY | O_CLOEXEC));
  ASSERT_TRUE(fd.is_valid());
  EXPECT_EQ(AttributeCheckStatus::NO_ATTR,
            CheckFileAttributes(path, false, fd.get()));

  const base::FilePath dir = test_dir_.Append("dir");
  ASSERT_EQ(0, mkdir(dir.value().c_str(), 0700));
  fd.reset(open(dir.value().c_str(), O_RDONLY | O_CLOEXEC));
  ASSERT_TRUE(fd.is_valid());
  EXPECT_EQ(AttributeCheckStatus::NO_ATTR,
            CheckFileAttributes(dir, false, fd.get()));
}

// Clear files w/the immutable bit set.
TEST_F(CheckFileAttributesTest, ResetFile) {
  if (!CanSetFileAttributes()) {
    SUCCEED();
    return;
  }

  const base::FilePath path = test_dir_.Append("file");
  ASSERT_TRUE(CreateFile(path, ""));

  base::ScopedFD fd(open(path.value().c_str(), O_RDONLY | O_CLOEXEC));
  ASSERT_TRUE(fd.is_valid());

  long flags;  // NOLINT(runtime/int)
  EXPECT_EQ(0, ioctl(fd.get(), FS_IOC_GETFLAGS, &flags));
  flags |= FS_IMMUTABLE_FL;
  EXPECT_EQ(0, ioctl(fd.get(), FS_IOC_SETFLAGS, &flags));

  EXPECT_EQ(AttributeCheckStatus::CLEARED,
            CheckFileAttributes(path, false, fd.get()));
}

// Clear dirs w/the immutable bit set.
TEST_F(CheckFileAttributesTest, ResetDir) {
  if (!CanSetFileAttributes()) {
    SUCCEED();
    return;
  }

  const base::FilePath dir = test_dir_.Append("dir");
  ASSERT_EQ(0, mkdir(dir.value().c_str(), 0700));

  base::ScopedFD fd(open(dir.value().c_str(), O_RDONLY | O_CLOEXEC));
  ASSERT_TRUE(fd.is_valid());

  long flags;  // NOLINT(runtime/int)
  EXPECT_EQ(0, ioctl(fd.get(), FS_IOC_GETFLAGS, &flags));
  flags |= FS_IMMUTABLE_FL;
  EXPECT_EQ(0, ioctl(fd.get(), FS_IOC_SETFLAGS, &flags));

  EXPECT_EQ(AttributeCheckStatus::CLEARED,
            CheckFileAttributes(dir, false, fd.get()));
}

namespace {

class RemoveURLExtendedAttributesTest : public ::testing::Test {
  void SetUp() {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_dir_ = scoped_temp_dir_.GetPath();
  }

 protected:
  base::FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;
};

}  // namespace

// Don't fail when files don't have extended attributes.
TEST_F(RemoveURLExtendedAttributesTest, NoAttributesSucceeds) {
  const base::FilePath path = test_dir_.Append("xattr");
  ASSERT_TRUE(CreateFile(path, ""));
  EXPECT_EQ(AttributeCheckStatus::NO_ATTR, RemoveURLExtendedAttributes(path));
}

// Clear files with the "xdg" xattrs set, see crbug.com/919486.
TEST_F(RemoveURLExtendedAttributesTest, Success) {
  const base::FilePath path = test_dir_.Append("xattr");
  ASSERT_TRUE(CreateFile(path, ""));
  const char* path_cstr = path.value().c_str();

  EXPECT_EQ(
      0, setxattr(path_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0, 0));
  EXPECT_EQ(
      0, setxattr(path_cstr, file_attrs_cleaner::xdg_referrer_url, NULL, 0, 0));

  EXPECT_EQ(AttributeCheckStatus::CLEARED, RemoveURLExtendedAttributes(path));

  // getxattr(2) call should fail now.
  EXPECT_GT(0,
            getxattr(path_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0));
  EXPECT_GT(0,
            getxattr(path_cstr, file_attrs_cleaner::xdg_referrer_url, NULL, 0));
}

// Leave other attributes alone.
TEST_F(RemoveURLExtendedAttributesTest, OtherAttributesUnchanged) {
  const base::FilePath path = test_dir_.Append("xattr");
  ASSERT_TRUE(CreateFile(path, ""));

  EXPECT_EQ(0, setxattr(path.value().c_str(), "user.test", NULL, 0, 0));
  EXPECT_EQ(AttributeCheckStatus::NO_ATTR, RemoveURLExtendedAttributes(path));

  // getxattr(2) call should succeed.
  EXPECT_EQ(0, getxattr(path.value().c_str(), "user.test", NULL, 0));
}

namespace {

class ScanDirTest : public ::testing::Test {
  void SetUp() {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_dir_ = scoped_temp_dir_.GetPath();
    url_xattrs_count_ = 0;
  }

 protected:
  base::FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;
  int url_xattrs_count_;
};

}  // namespace

TEST_F(ScanDirTest, Empty) {
  EXPECT_TRUE(ScanDir(test_dir_, {}, &url_xattrs_count_));
}

TEST_F(ScanDirTest, Leaf) {
  CreateFile(test_dir_.Append("file1"), "");
  CreateFile(test_dir_.Append("file2"), "");
  EXPECT_TRUE(ScanDir(test_dir_, {}, &url_xattrs_count_));
}

TEST_F(ScanDirTest, Nested) {
  CreateFile(test_dir_.Append("file1"), "");
  CreateFile(test_dir_.Append("file2"), "");
  EXPECT_TRUE(base::CreateDirectory(test_dir_.Append("emptydir")));

  const base::FilePath dir1(test_dir_.Append("dir1"));
  EXPECT_TRUE(base::CreateDirectory(dir1));
  CreateFile(dir1.Append("file1"), "");
  CreateFile(dir1.Append("file2"), "");
  EXPECT_TRUE(base::CreateDirectory(dir1.Append("emptydir")));

  const base::FilePath dir2(dir1.Append("dir1"));
  EXPECT_TRUE(base::CreateDirectory(dir2));
  CreateFile(dir2.Append("file1"), "");
  CreateFile(dir2.Append("file2"), "");
  EXPECT_TRUE(base::CreateDirectory(dir2.Append("emptydir")));

  EXPECT_TRUE(ScanDir(test_dir_, {}, &url_xattrs_count_));
}

TEST_F(ScanDirTest, RecurseAndClearAttributes) {
  const base::FilePath file1 = test_dir_.Append("file1");
  CreateFile(file1, "");
  CreateFile(test_dir_.Append("file1"), "");
  CreateFile(test_dir_.Append("file2"), "");

  const base::FilePath subdir(test_dir_.Append("subdir"));
  EXPECT_TRUE(base::CreateDirectory(subdir));
  const base::FilePath subfile1(subdir.Append("subfile1"));
  const base::FilePath subfile2(subdir.Append("subfile2"));
  const base::FilePath subfile3(subdir.Append("subfile3"));
  CreateFile(subfile1, "");
  CreateFile(subfile2, "");
  CreateFile(subfile3, "");

  const char* file1_cstr = file1.value().c_str();
  const char* subf1_cstr = subfile1.value().c_str();
  const char* subf3_cstr = subfile3.value().c_str();

  EXPECT_EQ(
      0, setxattr(file1_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0, 0));
  EXPECT_EQ(
      0, setxattr(subf1_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0, 0));
  EXPECT_EQ(
      0, setxattr(subf3_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0, 0));

  EXPECT_TRUE(ScanDir(test_dir_, {}, &url_xattrs_count_));

  EXPECT_EQ(url_xattrs_count_, 3);

  EXPECT_GT(0,
            getxattr(file1_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0));
  EXPECT_GT(0,
            getxattr(subf1_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0));
  EXPECT_GT(0,
            getxattr(subf3_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0));
}

TEST_F(ScanDirTest, SkipRecurse) {
  CreateFile(test_dir_.Append("file1"), "");
  CreateFile(test_dir_.Append("file2"), "");

  const base::FilePath subdir(test_dir_.Append("subdir"));
  EXPECT_TRUE(base::CreateDirectory(subdir));
  const base::FilePath subfile(subdir.Append("subfile"));
  CreateFile(subfile, "");
  const char* subf_cstr = subfile.value().c_str();

  EXPECT_EQ(
      0, setxattr(subf_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0, 0));

  std::vector<std::string> skip = {"subdir"};
  EXPECT_TRUE(ScanDir(test_dir_, skip, &url_xattrs_count_));

  EXPECT_EQ(0,
            getxattr(subf_cstr, file_attrs_cleaner::xdg_origin_url, NULL, 0));
}

TEST_F(ScanDirTest, InvalidDirSucceeds) {
  const base::FilePath subdir(
      test_dir_.Append("this_dir_definitely_does_not_exist"));

  EXPECT_TRUE(ScanDir(subdir, {}, &url_xattrs_count_));
}
