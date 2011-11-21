// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/system-mounter.h"

#include <errno.h>
#include <sys/mount.h>

#include <string>
#include <utility>

#include <base/logging.h>

using std::pair;
using std::string;

namespace cros_disks {

const char SystemMounter::kMounterType[] = "";

SystemMounter::SystemMounter(const string& source_path,
                             const string& target_path,
                             const string& filesystem_type,
                             const MountOptions& mount_options)
    : Mounter(source_path, target_path, filesystem_type, mount_options) {
}

MountErrorType SystemMounter::MountImpl() {
  pair<unsigned long, string> flags_and_data =
    mount_options().ToMountFlagsAndData();
  if (mount(source_path().c_str(), target_path().c_str(),
        filesystem_type().c_str(), flags_and_data.first,
        flags_and_data.second.c_str()) != 0) {
    PLOG(WARNING) << "mount('" << source_path() << "', '" << target_path()
      << "', '" << filesystem_type() << "', " << flags_and_data.first
      << ", '" << flags_and_data.second << "') failed";
    switch (errno) {
      case ENODEV:
        return MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
      case ENOENT:
      case ENOTBLK:
      case ENOTDIR:
        return MOUNT_ERROR_INVALID_PATH;
      case EPERM:
        return MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
      default:
        return MOUNT_ERROR_INTERNAL;
    }
  }
  return MOUNT_ERROR_NONE;
}

}  // namespace cros_disks
