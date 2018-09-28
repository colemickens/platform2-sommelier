// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/update_engine/utils.h>

#include <gtest/gtest.h>

namespace brillo {
namespace chromeos_update_engine {

class UtilsTest : public testing::Test {};

TEST_F(UtilsTest, SplitPartitionNameTest) {
  std::string disk;
  int part_num;

  EXPECT_TRUE(SplitPartitionName("/dev/sda3", &disk, &part_num));
  EXPECT_EQ("/dev/sda", disk);
  EXPECT_EQ(3, part_num);

  EXPECT_TRUE(SplitPartitionName("/dev/sdp1234", &disk, &part_num));
  EXPECT_EQ("/dev/sdp", disk);
  EXPECT_EQ(1234, part_num);

  EXPECT_TRUE(SplitPartitionName("/dev/mmcblk0p3", &disk, &part_num));
  EXPECT_EQ("/dev/mmcblk0", disk);
  EXPECT_EQ(3, part_num);

  EXPECT_TRUE(SplitPartitionName("/dev/ubiblock3_2", &disk, &part_num));
  EXPECT_EQ("/dev/ubiblock", disk);
  EXPECT_EQ(3, part_num);

  EXPECT_TRUE(SplitPartitionName("/dev/loop10", &disk, &part_num));
  EXPECT_EQ("/dev/loop", disk);
  EXPECT_EQ(10, part_num);

  EXPECT_TRUE(SplitPartitionName("/dev/loop28p11", &disk, &part_num));
  EXPECT_EQ("/dev/loop28", disk);
  EXPECT_EQ(11, part_num);

  EXPECT_TRUE(SplitPartitionName("/dev/loop10_0", &disk, &part_num));
  EXPECT_EQ("/dev/loop", disk);
  EXPECT_EQ(10, part_num);

  EXPECT_TRUE(SplitPartitionName("/dev/loop28p11_0", &disk, &part_num));
  EXPECT_EQ("/dev/loop28", disk);
  EXPECT_EQ(11, part_num);

  EXPECT_FALSE(SplitPartitionName("/dev/mmcblk0p", &disk, &part_num));
  EXPECT_FALSE(SplitPartitionName("/dev/sda", &disk, &part_num));
  EXPECT_FALSE(SplitPartitionName("/dev/foo/bar", &disk, &part_num));
  EXPECT_FALSE(SplitPartitionName("/", &disk, &part_num));
  EXPECT_FALSE(SplitPartitionName("", &disk, &part_num));
}

TEST_F(UtilsTest, SysfsBlockDeviceTest) {
  EXPECT_EQ("/sys/block/sda", SysfsBlockDevice("/dev/sda"));
  EXPECT_EQ("", SysfsBlockDevice("/foo/sda"));
  EXPECT_EQ("", SysfsBlockDevice("/dev/foo/bar"));
  EXPECT_EQ("", SysfsBlockDevice("/"));
  EXPECT_EQ("", SysfsBlockDevice("./"));
  EXPECT_EQ("", SysfsBlockDevice(""));
}

TEST_F(UtilsTest, GetPartitionNumberTest) {
  // The partition name should not be case-sensitive.
  EXPECT_EQ(2, GetPartitionNumber("kernel", 0, 2));
  EXPECT_EQ(2, GetPartitionNumber("boot", 0, 2));
  EXPECT_EQ(2, GetPartitionNumber("KERNEL", 0, 2));
  EXPECT_EQ(2, GetPartitionNumber("BOOT", 0, 2));

  EXPECT_EQ(3, GetPartitionNumber("ROOT", 0, 2));
  EXPECT_EQ(3, GetPartitionNumber("system", 0, 2));

  EXPECT_EQ(3, GetPartitionNumber("ROOT", 0, 2));
  EXPECT_EQ(3, GetPartitionNumber("system", 0, 2));

  // Slot B.
  EXPECT_EQ(4, GetPartitionNumber("KERNEL", 1, 2));
  EXPECT_EQ(5, GetPartitionNumber("ROOT", 1, 2));

  // Slot C doesn't exists.
  EXPECT_EQ(-1, GetPartitionNumber("KERNEL", 2, 2));
  EXPECT_EQ(-1, GetPartitionNumber("ROOT", 2, 2));

  // Non A/B partitions are ignored.
  EXPECT_EQ(-1, GetPartitionNumber("OEM", 0, 2));
  EXPECT_EQ(-1, GetPartitionNumber("A little panda", 0, 2));

  // Number of slots is too small.
  EXPECT_EQ(-1, GetPartitionNumber("kernel", 2, 2));
}

}  // namespace chromeos_update_engine
}  // namespace brillo
