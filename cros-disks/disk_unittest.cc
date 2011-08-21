// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk.h"

#include <gtest/gtest.h>

using std::string;

namespace cros_disks {

class DiskTest : public ::testing::Test {
 protected:
  Disk disk_;
};

TEST_F(DiskTest, GetPresentationNameForDiskWithoutLabelOrUuid) {
  EXPECT_EQ("Untitled", disk_.GetPresentationName());
}

TEST_F(DiskTest, GetPresentationNameForDiskWithLabelButNoUuid) {
  disk_.set_label("My Disk");
  EXPECT_EQ(disk_.label(), disk_.GetPresentationName());
}

TEST_F(DiskTest, GetPresentationNameForDiskWithUuidButNoLabel) {
  disk_.set_uuid("1A2B-3C4D");
  EXPECT_EQ(disk_.uuid(), disk_.GetPresentationName());
}

TEST_F(DiskTest, GetPresentationNameForDiskWithLabelAndUuid) {
  disk_.set_label("My Disk");
  disk_.set_uuid("1A2B-3C4D");
  EXPECT_EQ(disk_.label(), disk_.GetPresentationName());
}

TEST_F(DiskTest, GetPresentationNameForDiskWithLabelWithSlashes) {
  disk_.set_label("This/Is/My/Disk");
  EXPECT_EQ("This_Is_My_Disk", disk_.GetPresentationName());
}

TEST_F(DiskTest, GetPresentationNameForDiskWithUuidWithSlashes) {
  disk_.set_uuid("1A/2B/3C/4D");
  EXPECT_EQ("1A_2B_3C_4D", disk_.GetPresentationName());
}

}  // namespace cros_disks
