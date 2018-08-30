// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/system_mounter.h"

#include <sys/mount.h>

#include <string>

#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"

using std::string;

namespace cros_disks {

TEST(SystemMounterTest, RunAsRootMount) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Platform platform;
  SystemMounter mounter("/dev/null", temp_dir.GetPath().value(), "tmpfs",
                        MountOptions(), &platform);
  EXPECT_EQ(MOUNT_ERROR_NONE, mounter.Mount());
  umount2(temp_dir.GetPath().value().c_str(), MNT_FORCE);
}

TEST(SystemMounterTest, RunAsRootMountWithNonexistentSourcePath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // To test mounting a nonexistent source path, use ext2 as the
  // filesystem type instead of tmpfs since tmpfs does not care
  // about source path.
  Platform platform;
  SystemMounter mounter("/nonexistent", temp_dir.GetPath().value(), "ext2",
                        MountOptions(), &platform);
  EXPECT_EQ(MOUNT_ERROR_INVALID_PATH, mounter.Mount());
}

TEST(SystemMounterTest, RunAsRootMountWithNonexistentTargetPath) {
  Platform platform;
  SystemMounter mounter("/dev/null", "/nonexistent", "tmpfs", MountOptions(),
                        &platform);
  EXPECT_EQ(MOUNT_ERROR_INVALID_PATH, mounter.Mount());
}

TEST(SystemMounterTest, RunAsRootMountWithNonexistentFilesystemType) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Platform platform;
  SystemMounter mounter("/dev/null", temp_dir.GetPath().value(),
                        "nonexistentfs", MountOptions(), &platform);
  EXPECT_EQ(MOUNT_ERROR_UNSUPPORTED_FILESYSTEM, mounter.Mount());
}

}  // namespace cros_disks
