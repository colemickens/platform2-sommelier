// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount_entry.h"

using std::string;

namespace cros_disks {

MountEntry::MountEntry(MountErrorType error_type,
                       const string& source_path,
                       MountSourceType source_type,
                       const string& mount_path)
    : error_type_(error_type),
      source_path_(source_path),
      source_type_(source_type),
      mount_path_(mount_path) {}

MountEntry::~MountEntry() {}

DBusMountEntry MountEntry::ToDBusFormat() const {
  return {error_type_, source_path_, source_type_, mount_path_};
}

}  // namespace cros_disks
