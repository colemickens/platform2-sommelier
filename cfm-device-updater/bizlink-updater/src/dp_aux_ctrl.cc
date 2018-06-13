// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-updater/bizlink-updater/src/dp_aux_ctrl.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>

#include <vector>

using base::FileEnumerator;
using base::FilePath;

namespace {

const char kDrmCardPath[] = "/sys/class/drm/";

}  // namespace

namespace bizlink_updater {

void DrmDpAuxInit(std::vector<DrmPortInfo>* valid_drm_port_info) {
  DrmPortQuery(valid_drm_port_info);
  if (valid_drm_port_info->size() == 0) {
    LOG(ERROR) << "Didn't find valid DRM port.";
    return;
  }

  // TODO(frankhu): Check for the drm port for the dongle.
}

void DrmPortQuery(std::vector<DrmPortInfo>* valid_drm_port_info) {
  FilePath base_path(kDrmCardPath);

  // Query drm card.
  FileEnumerator card_enum(base_path, false, FileEnumerator::DIRECTORIES,
                           FILE_PATH_LITERAL("card?"));
  for (FilePath card_path = card_enum.Next(); !card_path.empty();
       card_path = card_enum.Next()) {
    // Query DP port.
    FileEnumerator dp_enum(card_path, false, FileEnumerator::DIRECTORIES,
                           FILE_PATH_LITERAL("card?-DP-?"));
    for (FilePath dp_path = dp_enum.Next(); !dp_path.empty();
         dp_path = dp_enum.Next()) {
      // Query I2C port.
      FileEnumerator i2c_enum(dp_path, false, FileEnumerator::DIRECTORIES,
                              FILE_PATH_LITERAL("i2c-?"));
      FilePath i2c_path = i2c_enum.Next();

      // Query drm_dp_aux port.
      FileEnumerator aux_enum(dp_path, false, FileEnumerator::DIRECTORIES,
                              FILE_PATH_LITERAL("drm_dp_aux?"));
      FilePath aux_path = aux_enum.Next();

      // Available DRM port with i2c and dp_aux.
      if ((!i2c_path.empty()) && (!aux_path.empty())) {
        DrmPortInfo drm_info = {
            card_path.value().back() - '0', dp_path.value().back() - '0',
            i2c_path.value().back() - '0', aux_path.value().back() - '0'};
        valid_drm_port_info->push_back(drm_info);
      }
    }
  }
}

}  // namespace bizlink_updater
