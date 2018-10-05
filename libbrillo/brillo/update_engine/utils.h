// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_UPDATE_ENGINE_UTILS_H_
#define LIBBRILLO_BRILLO_UPDATE_ENGINE_UTILS_H_

#include <climits>
#include <string>

#include <brillo/brillo_export.h>

namespace brillo {
namespace chromeos_update_engine {

// The directory in stateful partition where all DLCs are installed.
// TODO(xiaochu): supports install on both encrypted/unencrypted partitions.
BRILLO_EXPORT extern const char kDlcInstallRootDirectoryEncrypted[];
BRILLO_EXPORT extern const char kPartitionNamePrefixDlc[];
BRILLO_EXPORT extern const char kPartitionNameDlcA[];
BRILLO_EXPORT extern const char kPartitionNameDlcB[];
BRILLO_EXPORT extern const char kPartitionNameDlcImage[];

// Converts partition device name into device path in /sys/block.
BRILLO_EXPORT std::string SysfsBlockDevice(const std::string& device);

// Splits partition name into partition device name, and partition number.
// example:
// partition_name = "/dev/mmcblk0p3"
// out_disk_name -> "/dev/mmcblk0"
// partition number -> 3
BRILLO_EXPORT bool SplitPartitionName(const std::string& partition_name,
                                      std::string* out_disk_name,
                                      int* out_partition_num);

// Returns partition number given partition name and its slot.
BRILLO_EXPORT int GetPartitionNumber(const std::string& partition_name,
                                     unsigned int slot,
                                     int num_slots);

// Returns current slot the system is booted at.
BRILLO_EXPORT bool GetCurrentSlot(std::string* boot_disk_name,
                                  int* num_slots,
                                  unsigned int* current_slot);

}  // namespace chromeos_update_engine
}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_UPDATE_ENGINE_UTILS_H_
