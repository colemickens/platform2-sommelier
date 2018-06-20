// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_DP_AUX_CTRL_H_
#define CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_DP_AUX_CTRL_H_

#include "cfm-device-updater/bizlink-updater/src/mcdp_chip_ctrl.h"

#include <base/files/scoped_file.h>

namespace bizlink_updater {

bool GetValidDevice(McdpChipInfo* chip_info, int* dp_aux_port_id);
bool DrmAuxRead(const base::ScopedFD& fd,
                const off_t offset,
                size_t read_size,
                unsigned char* buf);
bool DrmAuxWrite(const base::ScopedFD& fd,
                 const off_t offset,
                 const size_t write_size,
                 const unsigned char* buf);
}  // namespace bizlink_updater

#endif  // CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_DP_AUX_CTRL_H_
