// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSTALLER_NAND_PARTITION_H_
#define INSTALLER_NAND_PARTITION_H_

#include <inttypes.h>
#include <string>

namespace brillo {

namespace installer {

// Remove the partition numbered |partno| from |dev|.
bool RemoveNandPartition(const std::string& dev, int partno);

// Add a partition to |dev|, starting from |offset|, for |length| bytes.
bool AddNandPartition(const std::string& dev,
                      int partno,
                      uint64_t offset,
                      uint64_t length);

}  // namespace installer

}  // namespace brillo

#endif  // INSTALLER_NAND_PARTITION_H_
