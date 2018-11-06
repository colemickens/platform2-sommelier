// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/boot_slot.h"

#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "dlcservice/mock_boot_device.h"

namespace dlcservice {

class BootSlotTest : public testing::Test {
 public:
  BootSlotTest() {
    auto mock_boot_device = std::make_unique<MockBootDevice>();
    boot_device_ = mock_boot_device.get();
    boot_slot_ = std::make_unique<BootSlot>(std::move(mock_boot_device));
  }

 protected:
  MockBootDevice* boot_device_;
  std::unique_ptr<BootSlot> boot_slot_;
};

TEST_F(BootSlotTest, SplitPartitionNameTest) {
  std::string disk;
  int part_num;

  EXPECT_TRUE(boot_slot_->SplitPartitionName("/dev/sda3", &disk, &part_num));
  EXPECT_EQ("/dev/sda", disk);
  EXPECT_EQ(3, part_num);

  EXPECT_TRUE(boot_slot_->SplitPartitionName("/dev/sdp1234", &disk, &part_num));
  EXPECT_EQ("/dev/sdp", disk);
  EXPECT_EQ(1234, part_num);

  EXPECT_TRUE(
      boot_slot_->SplitPartitionName("/dev/mmcblk0p3", &disk, &part_num));
  EXPECT_EQ("/dev/mmcblk0", disk);
  EXPECT_EQ(3, part_num);

  EXPECT_TRUE(
      boot_slot_->SplitPartitionName("/dev/ubiblock3_2", &disk, &part_num));
  EXPECT_EQ("/dev/ubiblock", disk);
  EXPECT_EQ(3, part_num);

  EXPECT_TRUE(boot_slot_->SplitPartitionName("/dev/loop10", &disk, &part_num));
  EXPECT_EQ("/dev/loop", disk);
  EXPECT_EQ(10, part_num);

  EXPECT_TRUE(
      boot_slot_->SplitPartitionName("/dev/loop28p11", &disk, &part_num));
  EXPECT_EQ("/dev/loop28", disk);
  EXPECT_EQ(11, part_num);

  EXPECT_TRUE(
      boot_slot_->SplitPartitionName("/dev/loop10_0", &disk, &part_num));
  EXPECT_EQ("/dev/loop", disk);
  EXPECT_EQ(10, part_num);

  EXPECT_TRUE(
      boot_slot_->SplitPartitionName("/dev/loop28p11_0", &disk, &part_num));
  EXPECT_EQ("/dev/loop28", disk);
  EXPECT_EQ(11, part_num);

  EXPECT_FALSE(
      boot_slot_->SplitPartitionName("/dev/mmcblk0p", &disk, &part_num));
  EXPECT_FALSE(boot_slot_->SplitPartitionName("/dev/sda", &disk, &part_num));
  EXPECT_FALSE(
      boot_slot_->SplitPartitionName("/dev/foo/bar", &disk, &part_num));
  EXPECT_FALSE(boot_slot_->SplitPartitionName("/", &disk, &part_num));
  EXPECT_FALSE(boot_slot_->SplitPartitionName("", &disk, &part_num));
}

TEST_F(BootSlotTest, GetPartitionNumberTest) {
  // The partition name should not be case-sensitive.
  EXPECT_EQ(2, boot_slot_->GetPartitionNumber("kernel", 0, 2));
  EXPECT_EQ(2, boot_slot_->GetPartitionNumber("KERNEL", 0, 2));

  EXPECT_EQ(3, boot_slot_->GetPartitionNumber("ROOT", 0, 2));

  EXPECT_EQ(3, boot_slot_->GetPartitionNumber("ROOT", 0, 2));

  // Slot B.
  EXPECT_EQ(4, boot_slot_->GetPartitionNumber("KERNEL", 1, 2));
  EXPECT_EQ(5, boot_slot_->GetPartitionNumber("ROOT", 1, 2));

  // Slot C doesn't exists.
  EXPECT_EQ(-1, boot_slot_->GetPartitionNumber("KERNEL", 2, 2));
  EXPECT_EQ(-1, boot_slot_->GetPartitionNumber("ROOT", 2, 2));

  // Non A/B partitions are ignored.
  EXPECT_EQ(-1, boot_slot_->GetPartitionNumber("OEM", 0, 2));
  EXPECT_EQ(-1, boot_slot_->GetPartitionNumber("A little panda", 0, 2));

  // Number of slots is too small.
  EXPECT_EQ(-1, boot_slot_->GetPartitionNumber("kernel", 2, 2));
}

TEST_F(BootSlotTest, GetCurrentSlotTest) {
  std::string boot_disk_name;
  int num_slots;
  int current_slot;

  // Boot from A slot.
  ON_CALL(*boot_device_, GetBootDevice())
      .WillByDefault(testing::Return("/dev/sda3"));
  ON_CALL(*boot_device_, IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(false));
  EXPECT_TRUE(
      boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot));
  EXPECT_EQ(boot_disk_name, "/dev/sda");
  EXPECT_EQ(num_slots, 2);
  EXPECT_EQ(current_slot, 0);

  // Boot from B slot.
  ON_CALL(*boot_device_, GetBootDevice())
      .WillByDefault(testing::Return("/dev/sda5"));
  ON_CALL(*boot_device_, IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(false));
  EXPECT_TRUE(
      boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot));
  EXPECT_EQ(boot_disk_name, "/dev/sda");
  EXPECT_EQ(num_slots, 2);
  EXPECT_EQ(current_slot, 1);

  // Boot from removable device.
  ON_CALL(*boot_device_, GetBootDevice())
      .WillByDefault(testing::Return("/dev/sdb3"));
  ON_CALL(*boot_device_, IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(true));
  EXPECT_TRUE(
      boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot));
  EXPECT_EQ(boot_disk_name, "/dev/sdb");
  EXPECT_EQ(num_slots, 1);
  EXPECT_EQ(current_slot, 0);

  // Boot from an invalid device.
  ON_CALL(*boot_device_, GetBootDevice())
      .WillByDefault(testing::Return("/dev/sda"));
  ON_CALL(*boot_device_, IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(true));
  EXPECT_FALSE(
      boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot));
}

}  // namespace dlcservice
