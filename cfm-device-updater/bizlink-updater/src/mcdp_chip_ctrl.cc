// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-updater/bizlink-updater/src/mcdp_chip_ctrl.h"
#include "cfm-device-updater/bizlink-updater/src/dp_aux_ctrl.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>

#include <fcntl.h>

#include <string>

namespace {

constexpr int kDpcdBranchIdStrLen = 6;
struct McdpChipBranchInfo {
  bizlink_updater::McdpChipId chip_id;
  bizlink_updater::McdpFwRunState fw_run_state;
  int str_len;
  unsigned char id_str[kDpcdBranchIdStrLen];
  int slave_addr;
};

const int kChipInfoFwVerByte2 = 7;
const int kChipInfoFwVerByte1 = 8;
const int kChipInfoFwVerByte0 = 5;
const int kRunIromChipTypeByteIdx = 4;
const int kOtherRunStateChipTypeByteIdx = 9;
const size_t kRegisterDataSize = 1;
const size_t kFlashFwChunkSize = 16;
const size_t kFlashEraseTimeLen = 2;
constexpr int kMcdpDpcdChipInfoLen = 14;
constexpr int kOuiDataLen = 3;

// Chip registers addresses.
const off_t kDpcdBranchIdStrAddr = 0x00503;
const off_t kPumaDpcdSinkModeReg = 0x0050D;
const off_t kPumaDpcdCmdStatusReg = 0x0050E;
const off_t kPumaAuxDpcdAddr = 0x80000;
const off_t kPumaEraseTimeAddr = 0x80004;
const off_t kPumaAuxDpcdEndAddr = 0x87FFF;
const off_t kDpcdOuiAddr = 0x00300;

// Chip status byte values.
const unsigned char kAuxFwUpdateRequest = 0xFE;
const unsigned char kAuxFwUpdateReady = 0xFC;
const unsigned char kAuxFwUpdateDone = 0xF8;
const unsigned char kAuxFwUpdateAbort = 0x55;
const unsigned char kAuxFlashInfoReady = 0xA1;
const unsigned char kAuxChunkReceived = 0x07;
const unsigned char kAuxChunkProcessed = 0x03;

const unsigned char kMcaOuiData0 = 0x00;
const unsigned char kMcaOuiData1 = 0x60;
const unsigned char kMcaOuiData2 = 0xAD;

// In PUMA App mode, it takes 18ms to get APP ISP driver ready.
const int kIspDriverReadyWaitTime = 18 * 1000;         // 18ms
const int kIspDriverReadyCheckInterval = 4 * 1000;     // 4ms
const int kDefaultFlashEraseWaitTime = 2000 * 1000;    // 2s
const int kIspUpdateReadyCheckInterval = 100 * 1000;   // 100ms
const int kAuxTransferStatusCheckInterval = 4 * 1000;  // 4ms
const int kAuxFwChunkProcessWaitTime = 100 * 1000;     // 100ms
const int kAuxChunkProcessCheckInterval = 50 * 1000;   // 50ms
const int kFwValidateWaitTime = 100 * 1000;            // 100ms
const int kFwValidateCheckInterval = 10 * 1000;        // 10ms
const int kWaitIspDriverTime = 5 * 1000;               // 5ms

// If a register doesn't get expected value after 100 checks, call it timeout.
const int kRegStatusMaxCheckCnt = 100;

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

const base::FilePath::StringType DP_AUX_DEV_PATH = "/dev/drm_dp_aux";

}  // namespace

namespace bizlink_updater {

bool AuxGetChipInfo(const base::ScopedFD& dev_fd, McdpChipInfo* chip_info) {
  unsigned char chip_info_str[kMcdpDpcdChipInfoLen];
  // Read device infomation from drm_ap_aux port.
  if (!DrmAuxRead(dev_fd, kDpcdBranchIdStrAddr, kMcdpDpcdChipInfoLen,
                  chip_info_str))
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

bool EnableUpdateMode(const base::ScopedFD& dev_fd, McdpFwRunState run_state) {
  int check_cnt = kRegStatusMaxCheckCnt;
  int erase_time = 0;
  unsigned char reg_status = 0;

  if (!DrmAuxWrite(dev_fd, kPumaDpcdSinkModeReg, kRegisterDataSize,
                   &kAuxFwUpdateRequest)) {
    LOG(ERROR) << "Failed to set device to FW update mode.";
    return false;
  }

  if (run_state == MCDP_RUN_APP) {
    usleep(kIspDriverReadyWaitTime);
    while ((reg_status != kAuxFlashInfoReady) && check_cnt--) {
      if (!DrmAuxRead(dev_fd, kPumaDpcdCmdStatusReg, kRegisterDataSize,
                      &reg_status)) {
        LOG(ERROR) << "Reading flash ID info failed.";
        return false;
      }
      usleep(kIspDriverReadyCheckInterval);
    }

    if (reg_status == kAuxFlashInfoReady) {
      if (!DrmAuxRead(dev_fd, kPumaEraseTimeAddr, kFlashEraseTimeLen,
                      reinterpret_cast<unsigned char*>(&erase_time))) {
        LOG(ERROR) << "Failed to read erase flash wait time.";
        erase_time = 0;
      }
    }
  }

  if (erase_time) {
    // erase_time is in ms while usleep() takes microsceconds.
    erase_time *= 1000;
  } else {
    erase_time = kDefaultFlashEraseWaitTime;
  }

  LOG(INFO) << "Erasing flash...";
  usleep(erase_time);
  check_cnt = kRegStatusMaxCheckCnt;

  while ((reg_status != kAuxFwUpdateReady) && check_cnt--) {
    if (!DrmAuxRead(dev_fd, kPumaDpcdSinkModeReg, 1, &reg_status)) {
      LOG(ERROR) << "Erase flash failed.";
      return false;
    }
    usleep(kIspUpdateReadyCheckInterval);
  }

  if (reg_status != kAuxFwUpdateReady) {
    LOG(ERROR) << "Erase flash timeout.";
    return false;
  }

  if (reg_status == kAuxFwUpdateReady) {
    LOG(INFO) << "Erase flash done.";
  }

  return true;
}

bool WriteFwThruAux(const base::ScopedFD& dev_fd,
                    unsigned char* fw_buf,
                    size_t fw_size) {
  size_t remain_size = fw_size;
  size_t trunk_size = 0;
  int chunk_num = 0;
  int write_addr = kPumaAuxDpcdAddr;

  LOG(INFO) << "Sending payload through aux...";
  while (remain_size) {
    trunk_size =
        (remain_size > kFlashFwChunkSize) ? kFlashFwChunkSize : remain_size;

    if (!DrmAuxWrite(dev_fd, write_addr, trunk_size, fw_buf)) {
      LOG(ERROR) << "Failed to send payload to AUX. Chunk #" << chunk_num
                 << " Sent " << fw_size - remain_size << " bytes.";
      return false;
    }
    fw_buf += trunk_size;
    write_addr += trunk_size;
    remain_size -= trunk_size;

    // Test for NACK issue. Give more time for ISP driver to change
    // dp_rx_aux_msg address.
    if ((write_addr & 0xFF) == 0) {
      usleep(kWaitIspDriverTime);
    }

    // Verify the chunk sent is received and processed.
    if (write_addr > kPumaAuxDpcdEndAddr) {
      write_addr = kPumaAuxDpcdAddr;
      chunk_num++;
      int wait_sink_cnt = kRegStatusMaxCheckCnt;
      unsigned char reg_status = 0;

      while ((reg_status != kAuxChunkReceived) &&
             (reg_status != kAuxChunkProcessed) && wait_sink_cnt--) {
        usleep(kAuxTransferStatusCheckInterval);
        if (!DrmAuxRead(dev_fd, kPumaDpcdCmdStatusReg, 1, &reg_status)) {
          LOG(ERROR) << "Failed to read DPCD cmd status for chunk #"
                     << chunk_num;
          return false;
        }
      }

      if (reg_status != kAuxChunkReceived && reg_status != kAuxChunkProcessed) {
        LOG(ERROR) << "Checking chunk #" << chunk_num
                   << " receiving status timeout.";
        return false;
      }

      if (reg_status != kAuxChunkProcessed) {
        usleep(kAuxFwChunkProcessWaitTime);
      }

      wait_sink_cnt = kRegStatusMaxCheckCnt;
      while (reg_status != kAuxChunkProcessed && wait_sink_cnt--) {
        usleep(kAuxChunkProcessCheckInterval);
        if (!DrmAuxRead(dev_fd, kPumaDpcdCmdStatusReg, 1, &reg_status)) {
          LOG(ERROR) << "Checking for chunk #" << chunk_num
                     << " processed status failed.";
          return false;
        }
      }

      if (reg_status != kAuxChunkProcessed) {
        LOG(ERROR) << "Waiting for processing chunk #" << chunk_num
                   << " timeout.";
        return false;
      }
    }
  }

  LOG(INFO) << "Send payload done.";
  return true;
}

bool ValidateFwUpdate(const base::ScopedFD& dev_fd) {
  LOG(INFO) << "Validating FW payload...";
  usleep(kFwValidateWaitTime);

  unsigned char reg_status = 0;
  int wait_sink_cnt = kRegStatusMaxCheckCnt;
  while (reg_status != kAuxFwUpdateDone && wait_sink_cnt--) {
    usleep(kFwValidateCheckInterval);
    if (!DrmAuxRead(dev_fd, kPumaDpcdSinkModeReg, 1, &reg_status)) {
      LOG(ERROR) << "Failed to read FW validating register.";
      return false;
    }
  }

  if (reg_status != kAuxFwUpdateDone) {
    LOG(ERROR) << "Validate FW payload timeout.";
    return false;
  }

  LOG(INFO) << "Validate FW payload done. FW update succeed.";
  return true;
}

bool AuxWriteMcaOui(const base::ScopedFD& dev_fd) {
  unsigned char oui_data[kOuiDataLen] = {kMcaOuiData0, kMcaOuiData1,
                                         kMcaOuiData2};
  if (!DrmAuxWrite(dev_fd, kDpcdOuiAddr, kOuiDataLen, oui_data)) {
    LOG(ERROR) << "Sending MCI OUI failed.";
    return false;
  }
  return true;
}

void AbortFwUpdate(const base::ScopedFD& dev_fd) {
  if (!DrmAuxWrite(dev_fd, kPumaDpcdCmdStatusReg, 1, &kAuxFwUpdateAbort)) {
    LOG(ERROR) << "Failed to abort fw update process.";
  }
}

void FlashNewFw(const base::FilePath& fw_path,
                const base::ScopedFD& dev_fd,
                const McdpChipInfo& device_info) {
  int64_t fw_size = 0;
  if (!base::GetFileSize(fw_path, &fw_size)) {
    LOG(ERROR) << "Failed to read FW size.";
    return;
  }
  unsigned char* fw_buf = new unsigned char[fw_size];
  int r = base::ReadFile(fw_path, (char*)fw_buf, fw_size);
  if (r != fw_size) {
    LOG(ERROR) << "Failed to load fw bin.";
    return;
  }

  if (!AuxWriteMcaOui(dev_fd)) {
    LOG(ERROR) << "Write mca oui data failed. Aborting...";
    AbortFwUpdate(dev_fd);
    return;
  }
  if (!EnableUpdateMode(dev_fd, device_info.fw_run_state)) {
    LOG(ERROR) << "Enable FW update mode failed. Aborting...";
    AbortFwUpdate(dev_fd);
    return;
  }
  if (!WriteFwThruAux(dev_fd, fw_buf, fw_size)) {
    LOG(ERROR) << "Flash FW failed. Aborting...";
    AbortFwUpdate(dev_fd);
    return;
  }
  if (ValidateFwUpdate(dev_fd) < 0) {
    LOG(ERROR) << "Validate FW failed. Aborting...";
    AbortFwUpdate(dev_fd);
    return;
  }
}

}  // namespace bizlink_updater
