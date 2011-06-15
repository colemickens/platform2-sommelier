// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/system-mounter.h"

#include <sys/mount.h>

#include <string>
#include <utility>

#include <base/logging.h>

namespace cros_disks {

SystemMounter::SystemMounter(const std::string& source_path,
    const std::string& target_path, const std::string& filesystem_type,
    const MountOptions& mount_options)
  : Mounter(source_path, target_path, filesystem_type, mount_options) {
}

bool SystemMounter::MountImpl() {
  std::pair<unsigned long, std::string> flags_and_data =
    mount_options().ToMountFlagsAndData();
  if (mount(source_path().c_str(), target_path().c_str(),
        filesystem_type().c_str(), flags_and_data.first,
        flags_and_data.second.c_str()) != 0) {
    PLOG(WARNING) << "mount('" << source_path() << "', '" << target_path()
      << "', '" << filesystem_type() << "', " << flags_and_data.first
      << ", '" << flags_and_data.second << "') failed";
    return false;
  }
  return true;
}

}  // namespace cros_disks
