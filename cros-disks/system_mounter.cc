// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/system_mounter.h"

#include <errno.h>
#include <sys/mount.h>

#include <string>
#include <utility>

#include <base/logging.h>

#include "cros-disks/platform.h"

using std::pair;
using std::string;

namespace cros_disks {

const char SystemMounter::kMounterType[] = "";

SystemMounter::SystemMounter(const string& source_path,
                             const string& target_path,
                             const string& filesystem_type,
                             const MountOptions& mount_options,
                             const Platform* platform)
    : MounterCompat(filesystem_type,
                    source_path,
                    base::FilePath(target_path),
                    mount_options),
      platform_(platform) {}

MountErrorType SystemMounter::MountImpl() const {
  auto flags_and_data = mount_options().ToMountFlagsAndData();
  return platform_->Mount(source(), target_path().value(), filesystem_type(),
                          flags_and_data.first, flags_and_data.second);
}

}  // namespace cros_disks
