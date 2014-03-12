// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/powerd/system/input.h"
#include "power_manager/powerd/system/udev_stub.h"

namespace power_manager {
namespace system {

class InputTest : public testing::Test {
 public:
  InputTest() {
    CHECK(input_dir_.CreateUniqueTempDir());
    input_.set_sysfs_input_path_for_testing(input_dir_.path());
    input_.Init(&prefs_, &udev_);
  }
  virtual ~InputTest() {}

 protected:
  FakePrefs prefs_;
  base::ScopedTempDir input_dir_;

  // Directory holding device information symlinked to from |input_dir_|.
  base::ScopedTempDir device_dir_;

  UdevStub udev_;
  Input input_;
};

TEST_F(InputTest, DetectUSBDevices) {
  // Test the detector on empty directory.
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());

  // Create a bunch of non-usb paths.
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../foo0/dev:1/00:00"),
                  input_dir_.path().Append("input0")));
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../bar4/dev:2/00:00"),
                  input_dir_.path().Append("input1")));
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../goo3/dev:3/00:00"),
                  input_dir_.path().Append("input2")));
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());

  // Create a "fake usb" path that contains "usb" as part of another word
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../busbreaker/00:00"),
                  input_dir_.path().Append("input3")));
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());

  // Create a true usb path.
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../usb3/dev:3/00:00"),
                  input_dir_.path().Append("input4")));
  EXPECT_TRUE(input_.IsUSBInputDeviceConnected());

  // Clear directory and create a usb path.
  ASSERT_TRUE(input_dir_.Delete());
  ASSERT_TRUE(input_dir_.CreateUniqueTempDir());
  input_.set_sysfs_input_path_for_testing(input_dir_.path());
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../usb/dev:5/00:00"),
                  input_dir_.path().Append("input10")));
  EXPECT_TRUE(input_.IsUSBInputDeviceConnected());

  // Clear directory and create a non-symlink usb path.  It should not counted
  // because all the input paths should be symlinks.
  ASSERT_TRUE(input_dir_.Delete());
  ASSERT_TRUE(input_dir_.CreateUniqueTempDir());
  input_.set_sysfs_input_path_for_testing(input_dir_.path());
  ASSERT_TRUE(base::CreateDirectory(input_dir_.path().Append("usb12")));
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());
}

TEST_F(InputTest, RegisterForUdevEvents) {
  scoped_ptr<Input> input(new Input);
  input->Init(&prefs_, &udev_);
  EXPECT_TRUE(udev_.HasObserver(Input::kInputUdevSubsystem, input.get()));

  Input* dead_ptr = input.get();
  input.reset();
  EXPECT_FALSE(udev_.HasObserver(Input::kInputUdevSubsystem, dead_ptr));
}

}  // namespace system
}  // namespace power_manager
