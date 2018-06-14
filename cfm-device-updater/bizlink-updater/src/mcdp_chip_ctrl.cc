// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-updater/bizlink-updater/src/mcdp_chip_ctrl.h"
#include "cfm-device-updater/bizlink-updater/src/dp_aux_ctrl.h"

#include <base/files/file_util.h>

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

namespace {

typedef enum {
  MCDP_CHIP_NONE = 0,
  MCDP_PUMA_2900 = 6,
  MCDP_PUMA_2920 = 7,
} McdpChipId;

typedef struct _FwAppSignIdInfo {
  McdpChipId chip_id;
  unsigned int sign_id_addr;
  unsigned char id_str[4];
} McdpFwAppSignIdInfo;

// FW version info starting address.
const int kPumaFwVerStartAddr = 0x8003E;

const int kFwBinVerStrLen = 3;
const int kFwBinVerByte2 = 1;
const int kFwBinVerByte1 = 0;
const int kFwBinVerByte0 = 2;

// Min FW size is 512KB.
const off_t kMinFwSize = (512 * 1024);
const size_t kFwSignLen = 4;
const McdpFwAppSignIdInfo kMcdpAppSignId = {
    MCDP_PUMA_2900, 0x080042UL, {'P', 'U', 'M', 'A'}};

}  // namespace

namespace bizlink_updater {

// Get firmware version infomation.
bool GetFwBinInfo(const base::FilePath& fw_path, uint32_t* version) {
  base::ScopedFD fw_bin_fd(open(fw_path.value().c_str(), O_RDONLY | O_CLOEXEC));
  if (fw_bin_fd.get() < 0) {
    PLOG(ERROR) << "Failed to open FW bin file";
    return false;
  }

  if (!VerifyFwBin(fw_bin_fd)) {
    LOG(ERROR) << "FW file verification failed.";
    return false;
  }

  uint8_t ver_str[kFwBinVerStrLen];
  if (!DrmAuxRead(fw_bin_fd, kPumaFwVerStartAddr, kFwBinVerStrLen, ver_str)) {
    LOG(ERROR) << "Read FW version failed.";
    return false;
  }

  *version = (ver_str[kFwBinVerByte2] << 16) | (ver_str[kFwBinVerByte1] << 8) |
             ver_str[kFwBinVerByte0];

  LOG(INFO) << "FW bin version: " << +ver_str[kFwBinVerByte2] << "."
            << +ver_str[kFwBinVerByte1] << "." << +ver_str[kFwBinVerByte0];

  return true;
}

// Verify the firmware binary by checking the size and FW signature.
bool VerifyFwBin(const base::ScopedFD& fd) {
  // Verify FW size.
  struct stat fw_stat;
  int r = fstat(fd.get(), &fw_stat);
  if (r < 0) {
    PLOG(ERROR) << "Check FW size failed";
    return false;
  }
  if (fw_stat.st_size < kMinFwSize) {
    LOG(ERROR) << "Invalid FW size: " << fw_stat.st_size;
    return false;
  }

  // Verify FW signature.
  unsigned char sign_id[kFwSignLen];
  if (!DrmAuxRead(fd, kMcdpAppSignId.sign_id_addr, kFwSignLen, sign_id)) {
    LOG(ERROR) << "Read FW signature failed.";
    return false;
  }
  if (memcmp(sign_id, kMcdpAppSignId.id_str, kFwSignLen) != 0) {
    LOG(ERROR) << "Failed to verify FW bin, wrong FW signature/ID."
               << "Expect: " << kMcdpAppSignId.id_str << ", get: " << sign_id;
    return false;
  }

  return true;
}

}  // namespace bizlink_updater
