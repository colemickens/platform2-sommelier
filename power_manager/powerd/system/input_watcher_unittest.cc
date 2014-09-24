// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/powerd/system/event_device_stub.h"
#include "power_manager/powerd/system/input_watcher.h"
#include "power_manager/powerd/system/udev_stub.h"

namespace power_manager {
namespace system {

class InputWatcherTest : public testing::Test {
 public:
  InputWatcherTest() {
    CHECK(input_dir_.CreateUniqueTempDir());
    input_watcher_.set_sysfs_input_path_for_testing(input_dir_.path());
    input_watcher_.Init(
        scoped_ptr<EventDeviceFactoryInterface>(new EventDeviceFactoryStub()),
        &prefs_, &udev_);
  }
  virtual ~InputWatcherTest() {}

 protected:
  FakePrefs prefs_;
  base::ScopedTempDir input_dir_;

  // Directory holding device information symlinked to from |input_dir_|.
  base::ScopedTempDir device_dir_;

  UdevStub udev_;
  InputWatcher input_watcher_;
};

TEST_F(InputWatcherTest, DetectUSBDevices) {
  // Test the detector on empty directory.
  EXPECT_FALSE(input_watcher_.IsUSBInputDeviceConnected());

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
  EXPECT_FALSE(input_watcher_.IsUSBInputDeviceConnected());

  // Create a "fake usb" path that contains "usb" as part of another word
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../busbreaker/00:00"),
                  input_dir_.path().Append("input3")));
  EXPECT_FALSE(input_watcher_.IsUSBInputDeviceConnected());

  // Create a true usb path.
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../usb3/dev:3/00:00"),
                  input_dir_.path().Append("input4")));
  EXPECT_TRUE(input_watcher_.IsUSBInputDeviceConnected());

  // Clear directory and create a usb path.
  ASSERT_TRUE(input_dir_.Delete());
  ASSERT_TRUE(input_dir_.CreateUniqueTempDir());
  input_watcher_.set_sysfs_input_path_for_testing(input_dir_.path());
  ASSERT_TRUE(base::CreateSymbolicLink(
                  input_dir_.path().Append("../../usb/dev:5/00:00"),
                  input_dir_.path().Append("input10")));
  EXPECT_TRUE(input_watcher_.IsUSBInputDeviceConnected());

  // Clear directory and create a non-symlink usb path.  It should not counted
  // because all the input paths should be symlinks.
  ASSERT_TRUE(input_dir_.Delete());
  ASSERT_TRUE(input_dir_.CreateUniqueTempDir());
  input_watcher_.set_sysfs_input_path_for_testing(input_dir_.path());
  ASSERT_TRUE(base::CreateDirectory(input_dir_.path().Append("usb12")));
  EXPECT_FALSE(input_watcher_.IsUSBInputDeviceConnected());
}

TEST_F(InputWatcherTest, RegisterForUdevEvents) {
  scoped_ptr<InputWatcher> input_watcher(new InputWatcher);
  input_watcher->set_sysfs_input_path_for_testing(input_dir_.path());
  input_watcher->Init(
      scoped_ptr<EventDeviceFactoryInterface>(new EventDeviceFactoryStub()),
      &prefs_, &udev_);
  EXPECT_TRUE(udev_.HasSubsystemObserver(InputWatcher::kInputUdevSubsystem,
                                         input_watcher.get()));

  InputWatcher* dead_ptr = input_watcher.get();
  input_watcher.reset();
  EXPECT_FALSE(udev_.HasSubsystemObserver(InputWatcher::kInputUdevSubsystem,
                                          dead_ptr));
}

}  // namespace system
}  // namespace power_manager
