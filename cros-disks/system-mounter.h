// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SYSTEM_MOUNTER_H_
#define CROS_DISKS_SYSTEM_MOUNTER_H_

#include <string>

#include "cros-disks/mounter.h"

namespace cros_disks {

// A class for mounting a device file using the system mount() call.
class SystemMounter : public Mounter {
 public:
  SystemMounter(const std::string& source_path,
      const std::string& target_path,
      const std::string& filesystem_type,
      const MountOptions& mount_options);

 protected:
  // Mounts a device file using the system mount() call.
  // This method returns true on success.
  virtual bool MountImpl();
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SYSTEM_MOUNTER_H_
