// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/service_constants.h"
#include "login_manager/wipe_mitigator.h"
#include "login_manager/system_utils.h"

namespace login_manager {

WipeMitigator::WipeMitigator(SystemUtils* system) : system_(system) {}

WipeMitigator::~WipeMitigator() {}

bool WipeMitigator::Mitigate() {
  system_->TouchResetFile();
  system_->AppendToClobberLog(OwnerKeyLossMitigator::kMitigateMsg);
  system_->SendSignalToPowerManager(power_manager::kRequestRestartSignal);
  return false;
}

}  // namespace login_manager
