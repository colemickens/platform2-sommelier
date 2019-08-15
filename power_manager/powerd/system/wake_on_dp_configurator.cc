// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/wake_on_dp_configurator.h"

#include <base/files/file_util.h>
#include <base/macros.h>

#include "power_manager/powerd/system/cros_ec_ioctl.h"

namespace power_manager {
namespace system {
namespace {

bool GetMkbpWakeMask(const base::ScopedFD& cros_ec_fd,
                     uint32_t* wake_mask_out) {
  DCHECK(cros_ec_fd.is_valid());
  DCHECK(wake_mask_out);
  if (cros_ec_fd.get() < 0)
    return false;
  cros_ec_ioctl::IoctlCommand<struct ec_params_mkbp_event_wake_mask,
                              struct ec_response_mkbp_event_wake_mask>
      cmd(EC_CMD_MKBP_WAKE_MASK);

  struct ec_params_mkbp_event_wake_mask mkbp_wake_mask_request_params = {};
  mkbp_wake_mask_request_params.action = GET_WAKE_MASK;
  mkbp_wake_mask_request_params.mask_type = EC_MKBP_EVENT_WAKE_MASK;
  cmd.SetReq(mkbp_wake_mask_request_params);
  if (!cmd.Run(cros_ec_fd.get())) {
    LOG(ERROR) << "Failed to get current MKBP wake mask. Result : "
               << cmd.Result();
    return false;
  }

  *wake_mask_out = cmd.Resp()->wake_mask;
  return true;
}

bool SetMkbpWakeMask(const base::ScopedFD& cros_ec_fd, uint32_t wake_mask) {
  DCHECK(cros_ec_fd.is_valid());
  if (cros_ec_fd.get() < 0)
    return false;
  cros_ec_ioctl::IoctlCommand<struct ec_params_mkbp_event_wake_mask,
                              cros_ec_ioctl::EmptyParam>
      cmd(EC_CMD_MKBP_WAKE_MASK);
  struct ec_params_mkbp_event_wake_mask mkbp_wake_mask_request_params = {};
  mkbp_wake_mask_request_params.action = SET_WAKE_MASK;
  mkbp_wake_mask_request_params.mask_type = EC_MKBP_EVENT_WAKE_MASK;
  mkbp_wake_mask_request_params.new_wake_mask = wake_mask;
  cmd.SetReq(mkbp_wake_mask_request_params);
  if (!cmd.Run(cros_ec_fd.get())) {
    LOG(ERROR) << "Failed to set new MKBP wake mask to " << wake_mask
               << cmd.Result();
    return false;
  }
  return true;
}

}  // namespace

void ConfigureWakeOnDp(bool enable) {
  uint32_t wake_mask;
  base::ScopedFD cros_ec_fd =
      base::ScopedFD(open(cros_ec_ioctl::kCrosEcDevNodePath, O_RDWR));

  if (!cros_ec_fd.is_valid()) {
    PLOG(ERROR) << "Failed to open " << cros_ec_ioctl::kCrosEcDevNodePath;
    return;
  }

  if (!GetMkbpWakeMask(cros_ec_fd, &wake_mask))
    return;

  if (enable)
    wake_mask |= EC_MKBP_EVENT_DP_ALT_MODE_ENTERED;
  else
    wake_mask &= ~EC_MKBP_EVENT_DP_ALT_MODE_ENTERED;

  if (SetMkbpWakeMask(cros_ec_fd, wake_mask))
    LOG(INFO) << "Wake on dp is " << (enable ? "enabled" : "disabled");
}

}  // namespace system
}  // namespace power_manager
