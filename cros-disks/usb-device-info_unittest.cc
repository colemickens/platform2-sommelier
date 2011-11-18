// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/usb-device-info.h"

#include <base/file_util.h>
#include <gtest/gtest.h>

using std::string;

namespace cros_disks {

class USBDeviceInfoTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    string content =
      "# This is a comment line\n"
      " \n"
      "\n"
      "18d1:4e11 mobile\n"
      "0bda:0138 sd\n";

    FilePath info_file;
    ASSERT_TRUE(file_util::CreateTemporaryFile(&info_file));
    ASSERT_EQ(content.size(),
              file_util::WriteFile(info_file, content.c_str(), content.size()));
    info_file_ = info_file.value();
  }

  virtual void TearDown() {
    ASSERT_TRUE(file_util::Delete(FilePath(info_file_), false));
  }

 protected:
  string info_file_;
  USBDeviceInfo info_;
};

TEST_F(USBDeviceInfoTest, GetDeviceMediaType) {
  EXPECT_EQ(DEVICE_MEDIA_USB, info_.GetDeviceMediaType("0bda", "0138"));

  EXPECT_TRUE(info_.RetrieveFromFile(info_file_));
  EXPECT_EQ(DEVICE_MEDIA_MOBILE, info_.GetDeviceMediaType("18d1", "4e11"));
  EXPECT_EQ(DEVICE_MEDIA_SD, info_.GetDeviceMediaType("0bda", "0138"));
  EXPECT_EQ(DEVICE_MEDIA_USB, info_.GetDeviceMediaType("1234", "5678"));
}

TEST_F(USBDeviceInfoTest, RetrieveFromFile) {
  EXPECT_TRUE(info_.RetrieveFromFile(info_file_));
}

TEST_F(USBDeviceInfoTest, ConvertToDeviceMediaType) {
  EXPECT_EQ(DEVICE_MEDIA_MOBILE, info_.ConvertToDeviceMediaType("mobile"));
  EXPECT_EQ(DEVICE_MEDIA_SD, info_.ConvertToDeviceMediaType("sd"));
  EXPECT_EQ(DEVICE_MEDIA_USB, info_.ConvertToDeviceMediaType("usb"));
  EXPECT_EQ(DEVICE_MEDIA_USB, info_.ConvertToDeviceMediaType(""));
  EXPECT_EQ(DEVICE_MEDIA_USB, info_.ConvertToDeviceMediaType("foo"));
}

}  // namespace cros_disks
