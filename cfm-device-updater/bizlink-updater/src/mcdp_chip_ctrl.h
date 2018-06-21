// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_MCDP_CHIP_CTRL_H_
#define CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_MCDP_CHIP_CTRL_H_

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>

namespace bizlink_updater {

// Megachips DisplayPort chip FW status.
enum McdpFwRunState {
  MCDP_RUN_NONE = 0,
  MCDP_RUN_IROM = 1,
  MCDP_RUN_BOOT_CODE = 2,
  MCDP_RUN_APP = 3,
};

// Megachips DisplayPort chip ID.
enum McdpChipId {
  MCDP_CHIP_NONE = 0,
  MCDP_PUMA_2900 = 6,
  MCDP_PUMA_2920 = 7,
};

struct McdpChipInfo {
  McdpChipId chip_id;
  McdpFwRunState fw_run_state;
  uint32_t fw_version;
  int slave_addr;
  int chip_type;
  int chip_rev;
  bool dual_bank_support;
  McdpChipInfo() {
    chip_id = MCDP_CHIP_NONE;
    fw_run_state = MCDP_RUN_NONE;
    fw_version = 0;
    slave_addr = -1;
    chip_type = -1;
    chip_rev = -1;
    dual_bank_support = false;
  }
};

bool AuxGetChipInfo(const base::ScopedFD& dev_fd, McdpChipInfo* chip_info);

bool FlashNewFw(const base::FilePath& fw_path,
                const base::ScopedFD& dev_fd,
                const McdpChipInfo& device_info);

}  // namespace bizlink_updater

#endif  // CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_MCDP_CHIP_CTRL_H_
