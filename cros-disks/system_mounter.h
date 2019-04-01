// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SYSTEM_MOUNTER_H_
#define CROS_DISKS_SYSTEM_MOUNTER_H_

#include <string>

#include <base/macros.h>

#include "cros-disks/mounter.h"

namespace cros_disks {

class Platform;

// A class for mounting a device file using the system mount() call.
class SystemMounter : public MounterCompat {
 public:
  // A unique type identifier of this derived mounter class.
  static const char kMounterType[];

  SystemMounter(const std::string& source_path,
                const std::string& target_path,
                const std::string& filesystem_type,
                const MountOptions& mount_options,
                const Platform* platform);

 protected:
  // Mounts a device file using the system mount() call.
  MountErrorType MountImpl() const override;

 private:
  const Platform* const platform_;

  DISALLOW_COPY_AND_ASSIGN(SystemMounter);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SYSTEM_MOUNTER_H_
