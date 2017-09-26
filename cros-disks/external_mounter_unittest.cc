// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/external_mounter.h"

#include <sys/mount.h>

#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "cros-disks/mount_options.h"

namespace cros_disks {

TEST(ExternalMounterTest, RunAsRootMount) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ExternalMounter mounter("/dev/null", temp_dir.path().value(), "tmpfs",
                          MountOptions());
  EXPECT_FALSE(mounter.GetMountProgramPath().empty());
  if (!mounter.GetMountProgramPath().empty()) {
    EXPECT_EQ(MOUNT_ERROR_NONE, mounter.Mount());
    umount2(temp_dir.path().value().c_str(), MNT_FORCE);
  } else {
    LOG(WARNING) << "Could not find an external mount program for testing.";
  }
}

TEST(ExternalMounterTest, RunAsRootMountWithNonexistentSourcePath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // To test mounting a nonexistent source path, use ext2 as the
  // filesystem type instead of tmpfs since tmpfs does not care
  // about source path.
  ExternalMounter mounter("/nonexistent", temp_dir.path().value(), "ext2",
                          MountOptions());
  EXPECT_EQ(MOUNT_ERROR_MOUNT_PROGRAM_FAILED, mounter.Mount());
}

TEST(ExternalMounterTest, RunAsRootMountWithNonexistentTargetPath) {
  ExternalMounter mounter("/dev/null", "/nonexistent", "tmpfs", MountOptions());
  EXPECT_EQ(MOUNT_ERROR_MOUNT_PROGRAM_FAILED, mounter.Mount());
}

TEST(ExternalMounterTest, RunAsRootMountWithNonexistentFilesystemType) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ExternalMounter mounter("/dev/null", temp_dir.path().value(), "nonexistentfs",
                          MountOptions());
  EXPECT_EQ(MOUNT_ERROR_MOUNT_PROGRAM_FAILED, mounter.Mount());
}

}  // namespace cros_disks
