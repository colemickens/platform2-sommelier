// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-updater/bizlink-updater/src/dp_aux_ctrl.h"
#include "cfm-device-updater/bizlink-updater/src/mcdp_chip_ctrl.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <vector>

using base::FileEnumerator;
using base::FilePath;

namespace {

struct DrmPortInfo {
  int card_id;
  int dp_port_id;
  int i2c_port_id;
  int dp_aux_port_id;
};

const char kDrmCardPath[] = "/sys/class/drm/";
const char kDpAuxDevPath[] = "/dev/drm_dp_aux";

}  // namespace

namespace bizlink_updater {

void DrmPortQuery(std::vector<DrmPortInfo>* drm_ports_info) {
  FilePath base_path(kDrmCardPath);

  // Query drm card.
  FileEnumerator card_enum(base_path, false, FileEnumerator::DIRECTORIES,
                           "card?");
  for (FilePath card_path = card_enum.Next(); !card_path.empty();
       card_path = card_enum.Next()) {
    // Query DP port.
    FileEnumerator dp_enum(card_path, false, FileEnumerator::DIRECTORIES,
                           "card?-DP-?");
    for (FilePath dp_path = dp_enum.Next(); !dp_path.empty();
         dp_path = dp_enum.Next()) {
      // Query I2C port.
      FileEnumerator i2c_enum(dp_path, false, FileEnumerator::DIRECTORIES,
                              "i2c-?");
      FilePath i2c_path = i2c_enum.Next();

      // Query drm_dp_aux port.
      FileEnumerator aux_enum(dp_path, false, FileEnumerator::DIRECTORIES,
                              "drm_dp_aux?");
      FilePath aux_path = aux_enum.Next();

      // Available DRM port with i2c and dp_aux.
      if ((!i2c_path.empty()) && (!aux_path.empty())) {
        DrmPortInfo drm_info = {
            card_path.value().back() - '0', dp_path.value().back() - '0',
            i2c_path.value().back() - '0', aux_path.value().back() - '0'};
        drm_ports_info->push_back(drm_info);
      }
    }
  }
}

bool GetValidDrmPort(std::vector<DrmPortInfo> ports,
                     McdpChipInfo* chip_info,
                     int* valid_port_id) {
  for (auto port : ports) {
    int dp_aux_id = port.dp_aux_port_id;
    LOG(INFO) << "Checking DP AUX port " << dp_aux_id;
    FilePath dev_path(kDpAuxDevPath + std::to_string(dp_aux_id));
    base::ScopedFD dev_fp(open(dev_path.value().c_str(), O_RDONLY | O_CLOEXEC));
    if (dev_fp.get() < 0) {
      LOG(ERROR) << "Failed to open port " << dp_aux_id;
      return false;
    }
    if (AuxGetChipInfo(dev_fp, chip_info)) {
      *valid_port_id = dp_aux_id;
      return true;
    }
  }
  LOG(ERROR) << "Didn't find valid DP AUX port.";
  return false;
}

bool GetValidDevice(McdpChipInfo* chip_info, int* valid_port_id) {
  std::vector<DrmPortInfo> drm_ports_info;
  DrmPortQuery(&drm_ports_info);
  if (drm_ports_info.size() == 0) {
    LOG(ERROR) << "Didn't find valid DRM port.";
    return false;
  }

  if (!GetValidDrmPort(drm_ports_info, chip_info, valid_port_id)) {
    return false;
  }

  return true;
}

bool DrmAuxRead(const base::ScopedFD& fd,
                const off_t offset,
                size_t read_size,
                unsigned char* buf) {
  // Read read_size bytes from aux dev at offset into buf.
  size_t size = pread(fd.get(), buf, read_size, offset);
  if (size != read_size) {
    PLOG(ERROR) << "Failed to read native aux data at offset " << offset;
    return false;
  }

  return true;
}

bool DrmAuxWrite(const base::ScopedFD& fd,
                 const off_t offset,
                 const size_t write_size,
                 const unsigned char* buf) {
  // Write write_size bytes from aux dev into buf.
  size_t size = pwrite(fd.get(), buf, write_size, offset);
  if (size != write_size) {
    PLOG(ERROR) << "Failed to write native aux data at offset " << offset;
    return false;
  }

  return true;
}

}  // namespace bizlink_updater
