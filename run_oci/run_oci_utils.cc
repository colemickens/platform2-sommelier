// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_oci/run_oci_utils.h"

#include <mntent.h>
#include <stdio.h>

#include <string>

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

}  // namespace run_oci
