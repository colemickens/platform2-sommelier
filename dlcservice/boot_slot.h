// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DLCSERVICE_BOOT_SLOT_H_
#define DLCSERVICE_BOOT_SLOT_H_

#include <memory>
#include <string>

#include <base/gtest_prod_util.h>
#include <base/macros.h>

namespace dlcservice {

class BootDeviceInterface;

class BootSlot {
 public:
  explicit BootSlot(std::unique_ptr<BootDeviceInterface> boot_device);
  ~BootSlot();

  // Gets the partition slot the system is currently booted from. Returns true
  // if the operation returns valid results, otherwise returns false.
  // |boot_disk_name_out| returns the device path of the disk the system is
  // booted from. For example, "/dev/sda". |num_slots_out| returns the number of
  // slots of the disk the system is booted from. |current_slot_out| returns the
  // slot the system is currently booted from.
  bool GetCurrentSlot(std::string* boot_disk_name_out,
                      int* num_slots_out,
                      int* current_slot_out);

 private:
  FRIEND_TEST(BootSlotTest, SplitPartitionNameTest);
  FRIEND_TEST(BootSlotTest, GetPartitionNumberTest);

  // Splits the partition device name into the block device name and partition
  // number. For example, "/dev/sda3" will be split into {"/dev/sda", 3} and
  // "/dev/mmcblk0p2" into {"/dev/mmcblk0", 2}
  // Returns false when malformed device name is passed in.
  // If both output parameters are omitted (null), can be used
  // just to test the validity of the device name. Note that the function
  // simply checks if the device name looks like a valid device, no other
  // checks are performed (i.e. it doesn't check if the device actually exists).
  bool SplitPartitionName(const std::string& partition_name,
                          std::string* disk_name_out,
                          int* partition_num_out);

  // Return the hard-coded partition number used in Chrome OS for the passed
  // |partition_name|, |slot| and |num_slots|. In case of invalid data, returns
  // -1.
  int GetPartitionNumber(const std::string& partition_name,
                         int slot,
                         int num_slots);

  std::unique_ptr<BootDeviceInterface> boot_device_;

  DISALLOW_COPY_AND_ASSIGN(BootSlot);
};

}  // namespace dlcservice

#endif  // DLCSERVICE_BOOT_SLOT_H_
