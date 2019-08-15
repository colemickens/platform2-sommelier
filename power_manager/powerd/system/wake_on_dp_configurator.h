// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_WAKE_ON_DP_CONFIGURATOR_H_
#define POWER_MANAGER_POWERD_SYSTEM_WAKE_ON_DP_CONFIGURATOR_H_

namespace power_manager {
namespace system {

// Configures "wake on DP hot plug event". If enabled, device will wake from
// suspend (S3/S0ix) if DP hot plug event is seen on a type-c port.
void ConfigureWakeOnDp(bool enable);

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_WAKE_ON_DP_CONFIGURATOR_H_
