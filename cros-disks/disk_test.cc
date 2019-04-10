// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk.h"

#include <gtest/gtest.h>

namespace cros_disks {

class DiskTest : public ::testing::Test {
 protected:
  Disk disk_;
};

TEST_F(DiskTest, GetPresentationNameForDiskWithLabel) {
  disk_.label = "My Disk";
  EXPECT_EQ(disk_.label, disk_.GetPresentationName());
}

TEST_F(DiskTest, GetPresentationNameForDiskWithLabelWithSlashes) {
  disk_.label = "This/Is/My/Disk";
  EXPECT_EQ("This_Is_My_Disk", disk_.GetPresentationName());
}

TEST_F(DiskTest, GetPresentationNameForDiskWithoutLabel) {
  EXPECT_EQ("External Drive", disk_.GetPresentationName());

  disk_.media_type = DEVICE_MEDIA_USB;
  EXPECT_EQ("USB Drive", disk_.GetPresentationName());

  disk_.media_type = DEVICE_MEDIA_SD;
  EXPECT_EQ("SD Card", disk_.GetPresentationName());

  disk_.media_type = DEVICE_MEDIA_OPTICAL_DISC;
  EXPECT_EQ("Optical Disc", disk_.GetPresentationName());

  disk_.media_type = DEVICE_MEDIA_MOBILE;
  EXPECT_EQ("Mobile Device", disk_.GetPresentationName());

  disk_.media_type = DEVICE_MEDIA_DVD;
  EXPECT_EQ("DVD", disk_.GetPresentationName());
}

}  // namespace cros_disks
