// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_SUSPEND_CONFIGURATOR_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_SUSPEND_CONFIGURATOR_STUB_H_

#include "power_manager/powerd/system/suspend_configurator.h"

#include <base/macros.h>

namespace power_manager {
namespace system {

// Stub implementation of SuspendConfiguratorInterface for use by tests.
class SuspendConfiguratorStub : public SuspendConfiguratorInterface {
 public:
  SuspendConfiguratorStub() = default;
  ~SuspendConfiguratorStub() override = default;

  // SuspendConfiguratorInterface implementation.
  void PrepareForSuspend() override {}
  void UndoPrepareForSuspend() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SuspendConfiguratorStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_SUSPEND_CONFIGURATOR_STUB_H_
