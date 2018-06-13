// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-updater/bizlink-updater/src/dp_aux_ctrl.h"

#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include <vector>

int main(int argc, char* argv[]) {
  // Configure logging to syslog.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  LOG(INFO) << "Bizlink FW updater start...";

  std::vector<bizlink_updater::DrmPortInfo> valid_drm_port_info;

  DrmDpAuxInit(&valid_drm_port_info);
  LOG(INFO) << "valid drm port num: " << valid_drm_port_info.size();
  for (int i = 0; i < valid_drm_port_info.size(); ++i) {
    LOG(INFO) << valid_drm_port_info[i].card_id << "\t"
              << valid_drm_port_info[i].dp_port_id << "\t"
              << valid_drm_port_info[i].i2c_port_id << "\t"
              << valid_drm_port_info[i].dp_aux_port_id;
  }

  return 0;
}
