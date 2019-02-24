// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dev-install/dev_install.h"

#include <unistd.h>

#include <istream>
#include <sstream>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace dev_install {

namespace {

class DevInstallMock : public DevInstall {
 public:
  MOCK_METHOD1(Exec, int(const std::vector<const char*>&));
  MOCK_CONST_METHOD0(IsDevMode, bool());
  MOCK_METHOD2(PromptUser, bool(std::istream&, const std::string&));
};

class DevInstallTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Set the default to dev mode enabled.  Most tests want that.
    ON_CALL(dev_install_, IsDevMode()).WillByDefault(Return(true));
  }

 protected:
  DevInstallMock dev_install_;
};

}  // namespace

// Check default run through.
TEST_F(DevInstallTest, Run) {
  EXPECT_CALL(dev_install_, Exec(_)).WillOnce(Return(1234));
  EXPECT_EQ(1234, dev_install_.Run());
}

// Systems not in dev mode should abort.
TEST_F(DevInstallTest, NonDevMode) {
  EXPECT_CALL(dev_install_, IsDevMode()).WillOnce(Return(false));
  EXPECT_CALL(dev_install_, Exec(_)).Times(0);
  EXPECT_EQ(2, dev_install_.Run());
}

namespace {

class PromptUserTest : public ::testing::Test {
 protected:
  DevInstall dev_install_;
};

}  // namespace

// The --yes flag should pass w/out prompting the user.
TEST_F(PromptUserTest, Forced) {
  dev_install_.SetYesForTest(true);
  std::stringstream stream("");
  EXPECT_TRUE(dev_install_.PromptUser(stream, ""));
}

// EOF input should fail.
TEST_F(PromptUserTest, Eof) {
  std::stringstream stream("");
  EXPECT_FALSE(dev_install_.PromptUser(stream, ""));
}

// Default input (hitting enter) should fail.
TEST_F(PromptUserTest, Default) {
  std::stringstream stream("\n");
  EXPECT_FALSE(dev_install_.PromptUser(stream, ""));
}

// Entering "n" should fail.
TEST_F(PromptUserTest, No) {
  std::stringstream stream("n\n");
  EXPECT_FALSE(dev_install_.PromptUser(stream, ""));
}

// Entering "y" should pass.
TEST_F(PromptUserTest, Yes) {
  std::stringstream stream("y\n");
  EXPECT_TRUE(dev_install_.PromptUser(stream, ""));
}

namespace {

class DeletePathTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_dir_ = scoped_temp_dir_.GetPath();
    dev_install_.SetStateDirForTest(test_dir_);
  }

 protected:
  DevInstall dev_install_;
  base::FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;
};

}  // namespace

// Check missing dir.
TEST_F(DeletePathTest, Missing) {
  struct stat st = {};
  EXPECT_TRUE(dev_install_.DeletePath(st, test_dir_.Append("foo")));
}

// Check deleting dir contents leaves the dir alone.
TEST_F(DeletePathTest, Empty) {
  struct stat st = {};
  EXPECT_TRUE(dev_install_.DeletePath(st, test_dir_));
  EXPECT_TRUE(base::PathExists(test_dir_));
}

// Check mounted deletion.
TEST_F(DeletePathTest, Mounted) {
  struct stat st = {};
  const base::FilePath subdir = test_dir_.Append("subdir");
  EXPECT_TRUE(base::CreateDirectory(subdir));
  EXPECT_FALSE(dev_install_.DeletePath(st, test_dir_));
  EXPECT_TRUE(base::PathExists(subdir));
}

// Check recursive deletion.
TEST_F(DeletePathTest, Works) {
  struct stat st;
  EXPECT_EQ(0, stat(test_dir_.value().c_str(), &st));

  EXPECT_EQ(3, base::WriteFile(test_dir_.Append("file"), "123", 3));
  EXPECT_EQ(0, symlink("x", test_dir_.Append("broken-sym").value().c_str()));
  EXPECT_EQ(0, symlink("file", test_dir_.Append("file-sym").value().c_str()));
  EXPECT_EQ(0, symlink(".", test_dir_.Append("dir-sym").value().c_str()));
  EXPECT_EQ(0, symlink("subdir", test_dir_.Append("dir-sym2").value().c_str()));
  const base::FilePath subdir = test_dir_.Append("subdir");
  EXPECT_TRUE(base::CreateDirectory(subdir));
  EXPECT_EQ(3, base::WriteFile(subdir.Append("file"), "123", 3));
  const base::FilePath subsubdir = test_dir_.Append("subdir");
  EXPECT_TRUE(base::CreateDirectory(subsubdir));
  EXPECT_EQ(3, base::WriteFile(subsubdir.Append("file"), "123", 3));

  EXPECT_TRUE(dev_install_.DeletePath(st, test_dir_));
  EXPECT_TRUE(base::PathExists(test_dir_));
  EXPECT_EQ(0, rmdir(test_dir_.value().c_str()));
}

}  // namespace dev_install
