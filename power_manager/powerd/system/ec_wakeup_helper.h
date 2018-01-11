// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_EC_WAKEUP_HELPER_H_
#define POWER_MANAGER_POWERD_SYSTEM_EC_WAKEUP_HELPER_H_

#include <base/files/file_util.h>
#include <base/macros.h>

#include "power_manager/powerd/system/ec_wakeup_helper_interface.h"

namespace power_manager {
namespace system {

class EcWakeupHelper : public EcWakeupHelperInterface {
 public:
  EcWakeupHelper();
  ~EcWakeupHelper() override;

  // Implementation of EcWakeupHelperInterface.
  bool IsSupported() override;
  bool AllowWakeupAsTablet(bool enabled) override;

 private:
  bool supported_;         // True iff EC supports angle-based wakeup controls.
  int cached_wake_angle_;  // EC wake angle cached from the last time we set it.
  base::FilePath sysfs_node_;  // Path of the sysfs node to write to.

  DISALLOW_COPY_AND_ASSIGN(EcWakeupHelper);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_EC_WAKEUP_HELPER_H_
