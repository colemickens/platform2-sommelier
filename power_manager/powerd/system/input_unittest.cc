// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/powerd/system/input.h"

namespace power_manager {
namespace system {

class InputTest : public testing::Test {
 public:
  InputTest() {
    CHECK(input_dir_.CreateUniqueTempDir());
    input_.set_sysfs_input_path_for_testing(input_dir_.path());
    CHECK(drm_dir_.CreateUniqueTempDir());
    input_.set_sysfs_drm_path_for_testing(drm_dir_.path());
    CHECK(device_dir_.CreateUniqueTempDir());
    input_.Init(&prefs_);
  }
  virtual ~InputTest() {}

 protected:
  // Creates a directory named |device_name| in |device_dir_| and adds a symlink
  // to it in |drm_dir_|. Returns the path to the directory.
  base::FilePath CreateDrmDevice(const std::string& device_name) {
    base::FilePath device_path = device_dir_.path().Append(device_name);
    CHECK(file_util::CreateDirectory(device_path));
    CHECK(file_util::CreateSymbolicLink(
              device_path, drm_dir_.path().Append(device_name)));
    return device_path;
  }

  FakePrefs prefs_;
  base::ScopedTempDir input_dir_;
  base::ScopedTempDir drm_dir_;

  // Directory holding device information symlinked to from the above
  // directories.
  base::ScopedTempDir device_dir_;

  Input input_;
};

TEST_F(InputTest, DetectUSBDevices) {
  // Test the detector on empty directory.
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());

  // Create a bunch of non-usb paths.
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  input_dir_.path().Append("../../foo0/dev:1/00:00"),
                  input_dir_.path().Append("input0")));
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  input_dir_.path().Append("../../bar4/dev:2/00:00"),
                  input_dir_.path().Append("input1")));
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  input_dir_.path().Append("../../goo3/dev:3/00:00"),
                  input_dir_.path().Append("input2")));
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());

  // Create a "fake usb" path that contains "usb" as part of another word
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  input_dir_.path().Append("../../busbreaker/00:00"),
                  input_dir_.path().Append("input3")));
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());

  // Create a true usb path.
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  input_dir_.path().Append("../../usb3/dev:3/00:00"),
                  input_dir_.path().Append("input4")));
  EXPECT_TRUE(input_.IsUSBInputDeviceConnected());

  // Clear directory and create a usb path.
  ASSERT_TRUE(input_dir_.Delete());
  ASSERT_TRUE(input_dir_.CreateUniqueTempDir());
  input_.set_sysfs_input_path_for_testing(input_dir_.path());
  ASSERT_TRUE(file_util::CreateSymbolicLink(
                  input_dir_.path().Append("../../usb/dev:5/00:00"),
                  input_dir_.path().Append("input10")));
  EXPECT_TRUE(input_.IsUSBInputDeviceConnected());

  // Clear directory and create a non-symlink usb path.  It should not counted
  // because all the input paths should be symlinks.
  ASSERT_TRUE(input_dir_.Delete());
  ASSERT_TRUE(input_dir_.CreateUniqueTempDir());
  input_.set_sysfs_input_path_for_testing(input_dir_.path());
  ASSERT_TRUE(file_util::CreateDirectory(input_dir_.path().Append("usb12")));
  EXPECT_FALSE(input_.IsUSBInputDeviceConnected());
}

TEST_F(InputTest, IsDisplayConnected) {
  // False if there aren't any devices.
  EXPECT_FALSE(input_.IsDisplayConnected());

  // False if there's no status file.
  base::FilePath device_path = CreateDrmDevice("card0-DP-1");
  EXPECT_FALSE(input_.IsDisplayConnected());

  // False if the device reports that it's disconnected.
  const char kDisconnected[] = "disconnected";
  base::FilePath status_path = device_path.Append(Input::kDrmStatusFile);
  ASSERT_TRUE(file_util::WriteFile(status_path, kDisconnected,
      strlen(kDisconnected)));
  EXPECT_FALSE(input_.IsDisplayConnected());

  // True when the device's status goes to "connected".
  ASSERT_TRUE(file_util::WriteFile(status_path, Input::kDrmStatusConnected,
      strlen(Input::kDrmStatusConnected)));
  EXPECT_TRUE(input_.IsDisplayConnected());

  // A trailing newline should be okay.
  std::string kConnectedNewline(Input::kDrmStatusConnected);
  kConnectedNewline += "\n";
  ASSERT_TRUE(file_util::WriteFile(status_path, kConnectedNewline.c_str(),
      kConnectedNewline.size()));
  EXPECT_TRUE(input_.IsDisplayConnected());

  // True when one device is connected even if there's another disconnected
  // device.
  base::FilePath second_device_path = CreateDrmDevice("card0-DP-0");
  base::FilePath second_status_path =
      second_device_path.Append(Input::kDrmStatusFile);
  ASSERT_TRUE(file_util::WriteFile(second_status_path, kDisconnected,
      strlen(kDisconnected)));
  EXPECT_TRUE(input_.IsDisplayConnected());

  // Disconnect the original device and create a new device that has a
  // "connected" status but doesn't match the expected naming pattern for a
  // video card. IsDisplayConnected() should return false.
  ASSERT_TRUE(file_util::WriteFile(status_path, kDisconnected,
      strlen(kDisconnected)));
  base::FilePath misnamed_device_path = CreateDrmDevice("control32");
  base::FilePath misnamed_status_path =
      misnamed_device_path.Append(Input::kDrmStatusFile);
  ASSERT_TRUE(file_util::WriteFile(misnamed_status_path,
      kConnectedNewline.c_str(), kConnectedNewline.size()));
  EXPECT_FALSE(input_.IsDisplayConnected());
}

}  // namespace system
}  // namespace power_manager
