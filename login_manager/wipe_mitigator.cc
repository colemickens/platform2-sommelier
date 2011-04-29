// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/wipe_mitigator.h"

#include "chromeos/dbus/service_constants.h"
#include "login_manager/owner_key.h"
#include "login_manager/system_utils.h"

namespace login_manager {

WipeMitigator::WipeMitigator(SystemUtils* system)
    : system_(system),
      mitigating_(false) {
}

WipeMitigator::~WipeMitigator() {}

bool WipeMitigator::Mitigate(OwnerKey* ignored) {
  system_->TouchResetFile();
  system_->AppendToClobberLog(OwnerKeyLossMitigator::kMitigateMsg);
  system_->SendSignalToPowerManager(power_manager::kRequestRestartSignal);
  mitigating_ = true;
  return false;
}

bool WipeMitigator::Mitigating() {
  return mitigating_;
}

}  // namespace login_manager
