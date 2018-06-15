// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-updater/bizlink-updater/src/mcdp_chip_ctrl.h"
#include "cfm-device-updater/bizlink-updater/src/dp_aux_ctrl.h"

#include <base/files/file_util.h>

namespace {

constexpr int kDpcdBranchIdStrLen = 6;
struct McdpChipBranchInfo {
  bizlink_updater::McdpChipId chip_id;
  bizlink_updater::McdpFwRunState fw_run_state;
  int str_len;
  unsigned char id_str[kDpcdBranchIdStrLen];
  int slave_addr;
};

// Chip registers addresses.
const off_t kDpcdBranchIdStrAddr = 0x00503;

const int kChipInfoFwVerByte2 = 7;
const int kChipInfoFwVerByte1 = 8;
const int kChipInfoFwVerByte0 = 5;
const int kRunIromChipTypeByteIdx = 4;
const int kOtherRunStateChipTypeByteIdx = 9;
constexpr int kMcdpDpcdChipInfoLen = 14;

const McdpChipBranchInfo kMcdpChipBrandIdTable[] = {
    {bizlink_updater::MCDP_PUMA_2900,
     bizlink_updater::MCDP_RUN_IROM,
     4,
     {'P', 'U', 'M', 'A'},
     0},
    {bizlink_updater::MCDP_PUMA_2900,
     bizlink_updater::MCDP_RUN_APP,
     5,
     {'M', 'C', '2', '9', '0'},
     0},
    {bizlink_updater::MCDP_CHIP_NONE,
     bizlink_updater::MCDP_RUN_NONE,
     6,
     {' ', ' ', ' ', ' ', ' ', ' '},
     0}};

}  // namespace

namespace bizlink_updater {

bool AuxGetChipInfo(const base::ScopedFD& dev_fd, McdpChipInfo* chip_info) {
  unsigned char chip_info_str[kMcdpDpcdChipInfoLen];

  // Read device infomation from drm_ap_aux port.
  if (!bizlink_updater::DrmAuxRead(dev_fd, kDpcdBranchIdStrAddr,
                                   kMcdpDpcdChipInfoLen, chip_info_str))
    return false;

  // Parse device info.
  for (auto branch_id : kMcdpChipBrandIdTable) {
    if (memcmp(chip_info_str, branch_id.id_str, branch_id.str_len) == 0) {
      chip_info->chip_id = branch_id.chip_id;
      chip_info->fw_run_state = branch_id.fw_run_state;
      chip_info->slave_addr = branch_id.slave_addr;
      LOG(INFO) << "Found device branch id: " << branch_id.id_str;
      break;
    }
  }

  if (chip_info->chip_id == MCDP_CHIP_NONE) {
    LOG(ERROR) << "Failed to match device brand. Brand string: "
               << chip_info_str;
    return false;
  }

  if (chip_info->fw_run_state == MCDP_RUN_IROM) {
    chip_info->chip_type = chip_info_str[kRunIromChipTypeByteIdx];
  } else {
    chip_info->dual_bank_support = true;
    chip_info->chip_type = chip_info_str[kOtherRunStateChipTypeByteIdx];
  }

  chip_info->fw_version = (chip_info_str[kChipInfoFwVerByte2] << 16) |
                          (chip_info_str[kChipInfoFwVerByte1] << 8) |
                          chip_info_str[kChipInfoFwVerByte0];

  LOG(INFO) << "Device FW version: " << +chip_info_str[kChipInfoFwVerByte2]
            << "." << +chip_info_str[kChipInfoFwVerByte1] << "."
            << +chip_info_str[kChipInfoFwVerByte0];

  return true;
}

}  // namespace bizlink_updater
