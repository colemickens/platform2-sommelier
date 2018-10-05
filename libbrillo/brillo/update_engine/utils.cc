// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/update_engine/utils.h>

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <rootdev/rootdev.h>

namespace brillo {
namespace chromeos_update_engine {

namespace {

constexpr char kChromeOSPartitionNameKernel[] = "kernel";
constexpr char kChromeOSPartitionNameRoot[] = "root";
constexpr char kAndroidPartitionNameKernel[] = "boot";
constexpr char kAndroidPartitionNameRoot[] = "system";

// Returns the currently booted rootfs partition. "/dev/sda3", for example.
std::string GetBootDevice() {
  char boot_path[PATH_MAX];
  // Resolve the boot device path fully, including dereferencing through
  // dm-verity.
  int ret = rootdev(boot_path, sizeof(boot_path), true, /*full resolution*/
                    false /*do not remove partition #*/);
  if (ret < 0) {
    LOG(ERROR) << "rootdev failed to find the root device";
    return "";
  }
  LOG_IF(WARNING, ret > 0) << "rootdev found a device name with no device node";

  // This local variable is used to construct the return string and is not
  // passed around after use.
  return boot_path;
}

// Checks if device is a removable device.
bool IsRemovableDevice(const std::string& device) {
  std::string sysfs_block = SysfsBlockDevice(device);
  std::string removable;
  if (sysfs_block.empty() ||
      !base::ReadFileToString(base::FilePath(sysfs_block).Append("removable"),
                              &removable)) {
    return false;
  }
  base::TrimWhitespaceASCII(removable, base::TRIM_ALL, &removable);
  return removable == "1";
}

}  // namespace

const char kDlcInstallRootDirectoryEncrypted[] = "/home/chronos/dlc";
const char kPartitionNamePrefixDlc[] = "dlc_";
const char kPartitionNameDlcA[] = "dlc_a";
const char kPartitionNameDlcB[] = "dlc_b";
const char kPartitionNameDlcImage[] = "dlc.img";

std::string SysfsBlockDevice(const std::string& device) {
  base::FilePath device_path(device);
  if (device_path.DirName().value() != "/dev") {
    return "";
  }
  return base::FilePath("/sys/block").Append(device_path.BaseName()).value();
}

bool SplitPartitionName(const std::string& partition_name,
                        std::string* out_disk_name,
                        int* out_partition_num) {
  if (!base::StartsWith(partition_name, "/dev/",
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Invalid partition device name: " << partition_name;
    return false;
  }

  size_t last_nondigit_pos = partition_name.find_last_not_of("0123456789");
  if (last_nondigit_pos == std::string::npos ||
      (last_nondigit_pos + 1) == partition_name.size()) {
    LOG(ERROR) << "Unable to parse partition device name: " << partition_name;
    return false;
  }

  size_t partition_name_len = std::string::npos;
  if (partition_name[last_nondigit_pos] == '_') {
    // NAND block devices have weird naming which could be something
    // like "/dev/ubiblock2_0". We discard "_0" in such a case.
    size_t prev_nondigit_pos =
        partition_name.find_last_not_of("0123456789", last_nondigit_pos - 1);
    if (prev_nondigit_pos == std::string::npos ||
        (prev_nondigit_pos + 1) == last_nondigit_pos) {
      LOG(ERROR) << "Unable to parse partition device name: " << partition_name;
      return false;
    }

    partition_name_len = last_nondigit_pos - prev_nondigit_pos;
    last_nondigit_pos = prev_nondigit_pos;
  }

  if (out_disk_name) {
    // Special case for MMC devices which have the following naming scheme:
    // mmcblk0p2
    size_t disk_name_len = last_nondigit_pos;
    if (partition_name[last_nondigit_pos] != 'p' || last_nondigit_pos == 0 ||
        !isdigit(partition_name[last_nondigit_pos - 1])) {
      disk_name_len++;
    }
    *out_disk_name = partition_name.substr(0, disk_name_len);
  }

  if (out_partition_num) {
    std::string partition_str =
        partition_name.substr(last_nondigit_pos + 1, partition_name_len);
    *out_partition_num = atoi(partition_str.c_str());
  }
  return true;
}

int GetPartitionNumber(const std::string& partition_name,
                       unsigned int slot,
                       int num_slots) {
  if (slot >= num_slots) {
    LOG(ERROR) << "Invalid slot number: " << slot << ", we only have "
               << num_slots << " slot(s)";
    return -1;
  }

  // In Chrome OS, the partition numbers are hard-coded:
  //   KERNEL-A=2, ROOT-A=3, KERNEL-B=4, ROOT-B=4, ...
  // To help compatibility between different we accept both lowercase and
  // uppercase names in the ChromeOS or Brillo standard names.
  // See http://www.chromium.org/chromium-os/chromiumos-design-docs/disk-format
  std::string partition_lower = base::ToLowerASCII(partition_name);
  int base_part_num = 2 + 2 * slot;
  if (partition_lower == kChromeOSPartitionNameKernel ||
      partition_lower == kAndroidPartitionNameKernel)
    return base_part_num + 0;
  if (partition_lower == kChromeOSPartitionNameRoot ||
      partition_lower == kAndroidPartitionNameRoot)
    return base_part_num + 1;
  LOG(ERROR) << "Unknown Chrome OS partition name \"" << partition_name << "\"";
  return -1;
}

bool GetCurrentSlot(std::string* boot_disk_name,
                    int* num_slots,
                    unsigned int* current_slot) {
  std::string boot_device = GetBootDevice();
  if (boot_device.empty())
    return false;

  int partition_num;
  if (!SplitPartitionName(boot_device, boot_disk_name, &partition_num))
    return false;

  // All installed Chrome OS devices have two slots. We don't update removable
  // devices, so we will pretend we have only one slot in that case.
  if (IsRemovableDevice(*boot_disk_name)) {
    LOG(INFO)
        << "Booted from a removable device, pretending we have only one slot.";
    *num_slots = 1;
  } else {
    // TODO(deymo): Look at the actual number of slots reported in the GPT.
    *num_slots = 2;
  }

  // Search through the slots to see which slot has the partition_num we booted
  // from. This should map to one of the existing slots, otherwise something is
  // very wrong.
  *current_slot = 0;
  while (*current_slot < *num_slots &&
         partition_num != GetPartitionNumber(kChromeOSPartitionNameRoot,
                                             *current_slot, *num_slots)) {
    (*current_slot)++;
  }
  if (*current_slot >= *num_slots) {
    LOG(ERROR) << "Couldn't find the slot number corresponding to the "
                  "partition "
               << boot_device << ", number of slots: " << *num_slots
               << ". This device is not updateable.";
    *num_slots = 1;
    *current_slot = UINT_MAX;
    return false;
  }

  return false;
}

}  // namespace chromeos_update_engine
}  // namespace brillo
