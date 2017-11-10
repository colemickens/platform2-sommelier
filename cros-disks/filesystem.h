// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_FILESYSTEM_H_
#define CROS_DISKS_FILESYSTEM_H_

#include <string>
#include <vector>

namespace cros_disks {

struct Filesystem {
  explicit Filesystem(const std::string& type);

  // Filesystem type.
  const std::string type;

  // This variable is set to true if default user and group ID can be
  // specified for mounting the filesystem.
  bool accepts_user_and_group_id;

  // Extra mount options to specify when mounting the filesystem.
  std::vector<std::string> extra_mount_options;

  // This variable is set to true if the filesystem should be mounted
  // as read-only.
  bool is_mounted_read_only;

  // Filesystem type that is passed to the mounter when mounting the
  // filesystem. It is set to |type| by default but can be set to a different
  // value when needed.
  std::string mount_type;

  // Type of mounter to use for mounting the filesystem.
  std::string mounter_type;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_FILESYSTEM_H_
