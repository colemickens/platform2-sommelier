// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/system_mounter.h"

#include <sys/mount.h>

#include <string>

#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "cros-disks/mount_options.h"
#include "cros-disks/mount_point.h"
#include "cros-disks/platform.h"

using std::string;

namespace cros_disks {

TEST(SystemMounterTest, RunAsRootMount) {
  Platform platform;
  SystemMounter mounter("tmpfs", &platform);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MountErrorType error = MountErrorType::MOUNT_ERROR_NONE;
  auto mountpoint = mounter.Mount("/dev/null", temp_dir.GetPath(), {}, &error);
  EXPECT_TRUE(mountpoint);
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_NONE, error);
  error = mountpoint->Unmount();
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_NONE, error);
}

TEST(SystemMounterTest, RunAsRootMountWithNonexistentSourcePath) {
  Platform platform;
  SystemMounter mounter("ext2", &platform);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // To test mounting a nonexistent source path, use ext2 as the
  // filesystem type instead of tmpfs since tmpfs does not care
  // about source path.
  MountErrorType error = MountErrorType::MOUNT_ERROR_NONE;
  auto mountpoint =
      mounter.Mount("/nonexistent", temp_dir.GetPath(), {}, &error);
  EXPECT_FALSE(mountpoint);
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_INVALID_PATH, error);
}

TEST(SystemMounterTest, RunAsRootMountWithNonexistentTargetPath) {
  Platform platform;
  SystemMounter mounter("tmpfs", &platform);

  MountErrorType error = MountErrorType::MOUNT_ERROR_NONE;
  auto mountpoint =
      mounter.Mount("/dev/null", base::FilePath("/nonexistent"), {}, &error);
  EXPECT_FALSE(mountpoint);
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_INVALID_PATH, error);
}

TEST(SystemMounterTest, RunAsRootMountWithNonexistentFilesystemType) {
  Platform platform;
  SystemMounter mounter("nonexistentfs", &platform);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MountErrorType error = MountErrorType::MOUNT_ERROR_NONE;
  auto mountpoint = mounter.Mount("/dev/null", temp_dir.GetPath(), {}, &error);
  EXPECT_FALSE(mountpoint);
  EXPECT_EQ(MountErrorType::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM, error);
}

}  // namespace cros_disks
