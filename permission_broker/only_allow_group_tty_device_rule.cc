// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/only_allow_group_tty_device_rule.h"

#include <grp.h>
#include <libudev.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace permission_broker {

OnlyAllowGroupTtyDeviceRule::OnlyAllowGroupTtyDeviceRule(
    const std::string& group_name)
    : TtySubsystemUdevRule("OnlyAllowGroupTtyDeviceRule"),
      group_name_(group_name) {
}

Rule::Result OnlyAllowGroupTtyDeviceRule::ProcessTtyDevice(
    udev_device* device) {
  const char* const devnode = udev_device_get_devnode(device);
  if (devnode == NULL) {
    return DENY;
  }

  // Get gid for |devnode|.
  struct stat st;
  int ret = stat(devnode, &st);
  if (ret < 0) {
    return DENY;
  }

  // Get buffer size for getgrgid_r().
  int64_t getgr_res = sysconf(_SC_GETGR_R_SIZE_MAX);
  if (getgr_res < 0) {
    return DENY;
  }

  // Get group name.
  struct group gr;
  struct group* pgr = nullptr;
  size_t getgr_size = static_cast<size_t>(getgr_res);
  std::vector<char> getgr_buf(getgr_size);
  ret = getgrgid_r(st.st_gid, &gr, getgr_buf.data(), getgr_buf.size(), &pgr);
  if (ret != 0 || pgr == NULL) {
    return DENY;
  }

  return group_name_ == pgr->gr_name ? ALLOW : DENY;
}

}  // namespace permission_broker
