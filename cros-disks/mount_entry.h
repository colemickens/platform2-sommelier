// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_MOUNT_ENTRY_H_
#define CROS_DISKS_MOUNT_ENTRY_H_

#include <string>

#include <chromeos/dbus/service_constants.h>

namespace cros_disks {

struct MountEntry {
 public:
  MountEntry(MountErrorType error_type,
             const std::string& source_path,
             MountSourceType source_type,
             const std::string& mount_path,
             bool is_read_only)
      : error_type(error_type),
        source_path(source_path),
        source_type(source_type),
        mount_path(mount_path),
        is_read_only(is_read_only) {}

  ~MountEntry() = default;

  MountErrorType error_type;
  std::string source_path;
  MountSourceType source_type;
  std::string mount_path;
  bool is_read_only;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_MOUNT_ENTRY_H_
