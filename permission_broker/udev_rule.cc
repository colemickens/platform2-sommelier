// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/udev_rule.h"

#include <grp.h>
#include <libudev.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

using std::string;

namespace permission_broker {

// static
std::string UdevRule::GetDevNodeGroupName(udev_device* device) {
  const char* const devnode = udev_device_get_devnode(device);
  if (devnode == NULL) {
    return "";
  }

  // Get gid for |devnode|.
  struct stat st;
  int ret = stat(devnode, &st);
  if (ret < 0) {
    return "";
  }

  // Get buffer size for getgrgid_r().
  int64_t getgr_res = sysconf(_SC_GETGR_R_SIZE_MAX);
  if (getgr_res < 0) {
    return "";
  }

  // Get group name.
  struct group gr;
  struct group* pgr = nullptr;
  size_t getgr_size = static_cast<size_t>(getgr_res);
  std::vector<char> getgr_buf(getgr_size);
  ret = getgrgid_r(st.st_gid, &gr, getgr_buf.data(), getgr_buf.size(), &pgr);
  if (ret != 0 || pgr == NULL) {
    return "";
  }

  return string(pgr->gr_name);
}

UdevRule::UdevRule(const string& name)
    : Rule(name), udev_(udev_new()), enumerate_(udev_enumerate_new(udev_)) {}

UdevRule::~UdevRule() {
  udev_enumerate_unref(enumerate_);
  udev_unref(udev_);
}

Rule::Result UdevRule::Process(const string& path, int interface_id) {
  udev_enumerate_scan_devices(enumerate_);

  struct udev_list_entry* entry = NULL;
  udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate_)) {
    const char* const syspath = udev_list_entry_get_name(entry);
    struct udev_device* device = udev_device_new_from_syspath(udev_, syspath);

    const char* const devnode = udev_device_get_devnode(device);
    if (devnode && !strcmp(devnode, path.c_str())) {
      const Result result = ProcessDevice(device);
      udev_device_unref(device);
      return result;
    }

    udev_device_unref(device);
  }

  return IGNORE;
}

}  // namespace permission_broker
