// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mounter.h"

#include <base/logging.h>

using std::string;

namespace cros_disks {

Mounter::Mounter(const string& source_path,
                 const string& target_path,
                 const string& filesystem_type,
                 const MountOptions& mount_options)
    : filesystem_type_(filesystem_type),
      source_path_(source_path),
      target_path_(target_path),
      mount_options_(mount_options) {
  CHECK(!target_path_.empty()) << "Invalid target path";
  // |filesystem_type| can be empty if it is auto-detected or in case of
  // a bind mount.
  // |source_path| can be empty if it's implied for the particular filesystem.
}

MountErrorType Mounter::Mount() {
  MountErrorType error = MountImpl();

  if (error == MOUNT_ERROR_NONE) {
    LOG(INFO) << "Mounted '" << source_path_ << "' to '" << target_path_
              << "' as filesystem '" << filesystem_type_ << "' with options '"
              << mount_options_.ToString() << "'";
  } else {
    LOG(ERROR) << "Failed to mount '" << source_path_ << "' to '"
               << target_path_ << "' as filesystem '" << filesystem_type_
               << "' with options '" << mount_options_.ToString() << "'";
  }
  return error;
}

}  // namespace cros_disks
