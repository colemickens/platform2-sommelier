// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>

#include <base/logging.h>
#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "cros-disks/external-mounter.h"
#include "cros-disks/mount-options.h"

namespace cros_disks {

class ExternalMounterTest : public ::testing::Test {
};

TEST_F(ExternalMounterTest, RunAsRootMount) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ExternalMounter mounter("/dev/null", temp_dir.path().value(),
      "tmpfs", MountOptions());
  EXPECT_FALSE(mounter.GetMountProgramPath().empty());
  if (!mounter.GetMountProgramPath().empty()) {
    EXPECT_TRUE(mounter.Mount());
    umount2(temp_dir.path().value().c_str(), MNT_FORCE);
  } else {
    LOG(WARNING) << "Could not find an external mount program for testing.";
  }
}

TEST_F(ExternalMounterTest, RunAsRootMountWithNonexistentSourcePath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // To test mounting a nonexistent source path, use ext2 as the
  // filesystem type instead of tmpfs since tmpfs does not care
  // about source path.
  ExternalMounter mounter("/nonexistent", temp_dir.path().value(),
      "ext2", MountOptions());
  EXPECT_FALSE(mounter.Mount());
}

TEST_F(ExternalMounterTest, RunAsRootMountWithNonexistentTargetPath) {
  ExternalMounter mounter("/dev/null", "/nonexistent", "tmpfs", MountOptions());
  EXPECT_FALSE(mounter.Mount());
}

TEST_F(ExternalMounterTest, RunAsRootMountWithNonexistentFilesystemType) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ExternalMounter mounter("/dev/null", temp_dir.path().value(),
      "nonexistentfs", MountOptions());
  EXPECT_FALSE(mounter.Mount());
}

TEST_F(ExternalMounterTest, RunAsRootMountWithInvalidMountOptions) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::vector<std::string> options;
  options.push_back("this_is_an_invalid_option");
  MountOptions mount_options;
  mount_options.Initialize(options, false, false, "", "");
  ExternalMounter mounter("/dev/null", temp_dir.path().value(),
      "tmpfs", mount_options);
  EXPECT_FALSE(mounter.Mount());
}

}  // namespace cros_disks
