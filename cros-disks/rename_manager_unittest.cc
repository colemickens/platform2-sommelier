// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/rename_manager.h"
#include "cros-disks/platform.h"

#include <string>

#include <gtest/gtest.h>

namespace {

// Forbidden characters in volume name
const char kForbiddenCharacters[] = "*?.,;:/\\|+=<>[]\"'\t";

};  // namespace

namespace cros_disks {

class RenameManagerTest : public ::testing::Test {
 public:
  RenameManagerTest() : manager_(&platform_, nullptr) {}

 protected:
  Platform platform_;
  RenameManager manager_;
};

TEST_F(RenameManagerTest, ValidateParameters) {
  // Test long volume names
  EXPECT_EQ(RENAME_ERROR_LONG_NAME,
            manager_.ValidateParameters("ABCDEFGHIJKL", "vfat"));
  EXPECT_EQ(RENAME_ERROR_LONG_NAME,
            manager_.ValidateParameters("ABCDEFGHIJKLMNOP", "exfat"));

  // Test volume name length limits
  EXPECT_EQ(RENAME_ERROR_NONE,
            manager_.ValidateParameters("ABCDEFGHIJK", "vfat"));
  EXPECT_EQ(RENAME_ERROR_NONE,
            manager_.ValidateParameters("ABCDEFGHIJKLMNO", "exfat"));

  // Test unsupported file system type
  EXPECT_EQ(RENAME_ERROR_UNSUPPORTED_FILESYSTEM,
            manager_.ValidateParameters("ABC", "nonexistent-fs"));
}

TEST_F(RenameManagerTest, CanRename) {
  EXPECT_TRUE(manager_.CanRename("/dev/sda1"));
  EXPECT_TRUE(manager_.CanRename("/devices/block/sda/sda1"));
  EXPECT_TRUE(manager_.CanRename("/sys/devices/block/sda/sda1"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/disk1"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/disk1/"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/disk 1"));
  EXPECT_FALSE(manager_.CanRename("/media/archive/test.zip"));
  EXPECT_FALSE(manager_.CanRename("/media/archive/test.zip/"));
  EXPECT_FALSE(manager_.CanRename("/media/archive/test 1.zip"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/disk1/test.zip"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/disk1/test 1.zip"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/disk1/dir1/test.zip"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/test.zip/test1.zip"));
  EXPECT_FALSE(manager_.CanRename("/home/chronos/user/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanRename("/home/chronos/user/GCache/test1.zip"));
  EXPECT_FALSE(
      manager_.CanRename("/home/chronos"
                         "/u-0123456789abcdef0123456789abcdef01234567"
                         "/Downloads/test1.zip"));
  EXPECT_FALSE(
      manager_.CanRename("/home/chronos"
                         "/u-0123456789abcdef0123456789abcdef01234567"
                         "/GCache/test1.zip"));
  EXPECT_FALSE(manager_.CanRename(""));
  EXPECT_FALSE(manager_.CanRename("/tmp"));
  EXPECT_FALSE(manager_.CanRename("/media/removable"));
  EXPECT_FALSE(manager_.CanRename("/media/removable/"));
  EXPECT_FALSE(manager_.CanRename("/media/archive"));
  EXPECT_FALSE(manager_.CanRename("/media/archive/"));
  EXPECT_FALSE(manager_.CanRename("/home/chronos/user/Downloads"));
  EXPECT_FALSE(manager_.CanRename("/home/chronos/user/Downloads/"));
}

class RenameManagerVolumeNameTest : public RenameManagerTest,
                                    public ::testing::WithParamInterface<int> {
};

INSTANTIATE_TEST_CASE_P(AsciiRange,
                        RenameManagerVolumeNameTest,
                        testing::Range(0, 256));

TEST_P(RenameManagerVolumeNameTest, ValidateParameters) {
  char value = GetParam();
  std::string volume_name = std::string("ABC") + value;

  if (!std::isprint(value) || strchr(kForbiddenCharacters, value)) {
    // Test forbidden characters in volume name
    EXPECT_EQ(RENAME_ERROR_INVALID_CHARACTER,
              manager_.ValidateParameters(volume_name, "vfat"));
    EXPECT_EQ(RENAME_ERROR_INVALID_CHARACTER,
              manager_.ValidateParameters(volume_name, "exfat"));
  } else {
    // Test allowed characters in volume name
    EXPECT_EQ(RENAME_ERROR_NONE,
              manager_.ValidateParameters(volume_name, "vfat"));
    EXPECT_EQ(RENAME_ERROR_NONE,
              manager_.ValidateParameters(volume_name, "exfat"));
  }
}

}  // namespace cros_disks
