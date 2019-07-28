// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_CROS_EC_HELPER_H_
#define POWER_MANAGER_POWERD_SYSTEM_CROS_EC_HELPER_H_

#include <base/files/file_util.h>
#include <base/macros.h>

#include "power_manager/powerd/system/cros_ec_helper_interface.h"

namespace power_manager {
namespace system {

class CrosEcHelper : public CrosEcHelperInterface {
 public:
  CrosEcHelper();
  ~CrosEcHelper() override;

  // Implementation of EcHelperInterface.
  bool IsWakeAngleSupported() override;
  bool AllowWakeupAsTablet(bool enabled) override;

 private:
  // True iff EC supports angle-based wakeup controls.
  bool wake_angle_supported_ = false;
  // EC wake angle cached from the last time we set it.
  int cached_wake_angle_ = -1;
  // Path of the sysfs node to write to.
  base::FilePath wake_angle_sysfs_node_;

  DISALLOW_COPY_AND_ASSIGN(CrosEcHelper);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_CROS_EC_HELPER_H_
