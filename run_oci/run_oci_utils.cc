// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_oci/run_oci_utils.h"

#include <mntent.h>
#include <stdio.h>
#include <sys/capability.h>

#include <memory>
#include <string>
#include <type_traits>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/strings/string_split.h>

namespace run_oci {

std::vector<base::FilePath> GetMountpointsUnder(
    const base::FilePath& root, const base::FilePath& procSelfMountsPath) {
  base::ScopedFILE mountinfo(fopen(procSelfMountsPath.value().c_str(), "r"));
  if (!mountinfo) {
    PLOG(ERROR) << "Failed to open " << procSelfMountsPath.value();
    return std::vector<base::FilePath>();
  }

  struct mntent mount_entry;

  std::string line;
  char buffer[1024];
  std::vector<base::FilePath> mountpoints;
  while (getmntent_r(mountinfo.get(), &mount_entry, buffer, sizeof(buffer))) {
    // Only return paths that are under |root|.
    const std::string mountpoint = mount_entry.mnt_dir;
    if (mountpoint.compare(0, root.value().size(), root.value()) != 0)
      continue;
    mountpoints.emplace_back(mountpoint);
  }

  return mountpoints;
}

bool HasCapSysAdmin() {
  if (!CAP_IS_SUPPORTED(CAP_SYS_ADMIN))
    return false;

  std::unique_ptr<std::remove_pointer_t<cap_t>, decltype(&cap_free)> caps(
      cap_get_proc(), &cap_free);
  if (!caps) {
    PLOG(ERROR) << "Failed to get process' capabilities";
    return false;
  }

  cap_flag_value_t cap_value;
  if (cap_get_flag(caps.get(), CAP_SYS_ADMIN, CAP_EFFECTIVE, &cap_value) != 0) {
    PLOG(ERROR) << "Failed to get the value of CAP_SYS_ADMIN";
    return false;
  }
  return cap_value == CAP_SET;
}

}  // namespace run_oci
