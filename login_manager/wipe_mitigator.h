// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_WIPE_MITIGATOR_H_
#define LOGIN_MANAGER_WIPE_MITIGATOR_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "login_manager/owner_key_loss_mitigator.h"
#include "login_manager/system_utils.h"

namespace login_manager {

// This class mitigates owner key loss by triggering a wipe of the stateful
// partition and forcing a reboot.
class WipeMitigator : public OwnerKeyLossMitigator {
 public:
  WipeMitigator(SystemUtils* system);
  virtual ~WipeMitigator();

  // Deal with loss of the owner's private key.
  // Returning true means that we can recover without user interaction.
  // Returning false means that we can't.
  bool Mitigate();

 private:
  scoped_ptr<SystemUtils> system_;
  DISALLOW_COPY_AND_ASSIGN(WipeMitigator);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_WIPE_MITIGATOR_H_
