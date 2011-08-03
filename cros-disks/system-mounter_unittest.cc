// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>

#include <string>
#include <vector>

#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "cros-disks/mount-options.h"
#include "cros-disks/system-mounter.h"

using std::string;
using std::vector;

namespace cros_disks {

class SystemMounterTest : public ::testing::Test {
};

TEST_F(SystemMounterTest, RunAsRootMount) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  SystemMounter mounter("/dev/null", temp_dir.path().value(),
                        "tmpfs", MountOptions());
  EXPECT_EQ(kMountErrorNone, mounter.Mount());
  umount2(temp_dir.path().value().c_str(), MNT_FORCE);
}

TEST_F(SystemMounterTest, RunAsRootMountWithNonexistentSourcePath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // To test mounting a nonexistent source path, use ext2 as the
  // filesystem type instead of tmpfs since tmpfs does not care
  // about source path.
  SystemMounter mounter("/nonexistent", temp_dir.path().value(),
                        "ext2", MountOptions());
  EXPECT_EQ(kMountErrorInvalidPath, mounter.Mount());
}

TEST_F(SystemMounterTest, RunAsRootMountWithNonexistentTargetPath) {
  SystemMounter mounter("/dev/null", "/nonexistent", "tmpfs", MountOptions());
  EXPECT_EQ(kMountErrorInvalidPath, mounter.Mount());
}

TEST_F(SystemMounterTest, RunAsRootMountWithNonexistentFilesystemType) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  SystemMounter mounter("/dev/null", temp_dir.path().value(),
                        "nonexistentfs", MountOptions());
  EXPECT_EQ(kMountErrorUnsupportedFilesystem, mounter.Mount());
}

TEST_F(SystemMounterTest, RunAsRootMountWithInvalidMountOptions) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  vector<string> options;
  options.push_back("this_is_an_invalid_option");
  MountOptions mount_options;
  mount_options.Initialize(options, false, "", "");
  SystemMounter mounter("/dev/null", temp_dir.path().value(),
                        "tmpfs", mount_options);
  EXPECT_EQ(kMountErrorInternal, mounter.Mount());
}

}  // namespace cros_disks
