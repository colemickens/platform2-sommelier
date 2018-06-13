// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_DP_AUX_CTRL_H_
#define CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_DP_AUX_CTRL_H_

#include <vector>

namespace bizlink_updater {

typedef struct _DrmPortInfo {
  int card_id;
  int dp_port_id;
  int i2c_port_id;
  int dp_aux_port_id;
} DrmPortInfo;

void DrmPortQuery(std::vector<DrmPortInfo>* valid_drm_port_info);
void DrmDpAuxInit(std::vector<DrmPortInfo>* valid_drm_port_info);

}  // namespace bizlink_updater

#endif  // CFM_DEVICE_UPDATER_BIZLINK_UPDATER_SRC_DP_AUX_CTRL_H_
