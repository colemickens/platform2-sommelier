// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "power_manager/powerd/system/input.h"

namespace power_manager {
namespace system {

TEST(InputTest, DetectUSBDevices) {
  base::ScopedTempDir temp_dir;
  // Create temp directory to be used in place of the default sysfs path.
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Input input;
  input.set_sysfs_input_path_for_testing(temp_dir.path().value());

  // Test the detector on empty directory.
  EXPECT_FALSE(input.IsUSBInputDeviceConnected());

  // Create a bunch of non-usb paths.
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  temp_dir.path().Append("../../foo0/dev:1/00:00"),
                  temp_dir.path().Append("input0")));
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  temp_dir.path().Append("../../bar4/dev:2/00:00"),
                  temp_dir.path().Append("input1")));
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  temp_dir.path().Append("../../goo3/dev:3/00:00"),
                  temp_dir.path().Append("input2")));
  EXPECT_FALSE(input.IsUSBInputDeviceConnected());

  // Create a "fake usb" path that contains "usb" as part of another word
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  temp_dir.path().Append("../../busbreaker/00:00"),
                  temp_dir.path().Append("input3")));
  EXPECT_FALSE(input.IsUSBInputDeviceConnected());

  // Create a true usb path.
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  temp_dir.path().Append("../../usb3/dev:3/00:00"),
                  temp_dir.path().Append("input4")));
  EXPECT_TRUE(input.IsUSBInputDeviceConnected());

  // Clear directory and create a usb path.
  ASSERT_TRUE(temp_dir.Delete());
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  input.set_sysfs_input_path_for_testing(temp_dir.path().value());
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  temp_dir.path().Append("../../usb/dev:5/00:00"),
                  temp_dir.path().Append("input10")));
  EXPECT_TRUE(input.IsUSBInputDeviceConnected());

  // Clear directory and create a non-symlink usb path.  It should not counted
  // because all the input paths should be symlinks.
  ASSERT_TRUE(temp_dir.Delete());
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  input.set_sysfs_input_path_for_testing(temp_dir.path().value());
  ASSERT_TRUE(file_util::CreateDirectory(temp_dir.path().Append("usb12")));
  EXPECT_FALSE(input.IsUSBInputDeviceConnected());
}

}  // namespace system
}  // namespace power_manager
