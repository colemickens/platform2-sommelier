// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/filesystem.h"

#include <base/logging.h>

using std::string;

namespace cros_disks {

Filesystem::Filesystem(const string& type)
  : accepts_user_and_group_id_(false),
    is_experimental_(false),
    is_mounted_read_only_(false),
    requires_external_mounter_(false),
    type_(type) {
  CHECK(!type.empty()) << "Invalid filesystem type";
}

Filesystem::~Filesystem() {
}

}  // namespace cros_disks
