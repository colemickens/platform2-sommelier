// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mounter.h"

#include <base/logging.h>

namespace cros_disks {

Mounter::Mounter(const std::string& source_path,
    const std::string& target_path, const std::string& filesystem_type,
    const MountOptions& mount_options)
  : filesystem_type_(filesystem_type),
    source_path_(source_path),
    target_path_(target_path),
    mount_options_(mount_options) {
  CHECK(!source_path.empty()) << "Invalid source path";
  CHECK(!target_path.empty()) << "Invalid target path";
  CHECK(!filesystem_type.empty()) << "Invalid filesystem type";
}

Mounter::~Mounter() {
}

bool Mounter::Mount() {
  bool mounted = false;
  if (MountImpl()) {
    mounted = true;
  } else {
    // Try to mount the filesystem read-only if mounting it read-write failed.
    if (!mount_options_.IsReadOnlyOptionSet()) {
      LOG(INFO) << "Trying to mount '" << source_path_ << "' read-only";
      mount_options_.SetReadOnlyOption();
      if (MountImpl()) {
        mounted = true;
      }
    }
  }

  if (mounted) {
    LOG(INFO) << "Mounted '" << source_path_
      << "' to '" << target_path_
      << "' as filesystem '" << filesystem_type_
      << "' with options '" << mount_options_.ToString()
      << "'";
  } else {
    LOG(ERROR) << "Failed to mount '" << source_path_
      << "' to '" << target_path_
      << "' as filesystem '" << filesystem_type_
      << "' with options '" << mount_options_.ToString()
      << "'";
  }
  return mounted;
}

}  // namespace cros_disks
