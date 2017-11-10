// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/filesystem.h"

#include <base/logging.h>

namespace cros_disks {

Filesystem::Filesystem(const std::string& type)
    : type(type),
      accepts_user_and_group_id(false),
      is_mounted_read_only(false),
      mount_type(type) {
  CHECK(!type.empty()) << "Invalid filesystem type";
}

}  // namespace cros_disks
