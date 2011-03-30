// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_OWNER_KEY_LOSS_MITIGATOR_H_
#define LOGIN_MANAGER_OWNER_KEY_LOSS_MITIGATOR_H_

#include "base/basictypes.h"

namespace login_manager {

// Sometimes, the user we believe to be the Owner will not be able to
// demonstrate possession of the Owner private key.  This class defines the
// interface for objects that can handle this situation.
class OwnerKeyLossMitigator {
 public:
  static const char kMitigateMsg[];

  OwnerKeyLossMitigator();
  virtual ~OwnerKeyLossMitigator();

  // Deal with loss of the owner's private key.
  // Returning true means that we can recover without user interaction.
  // Returning false means that we can't.
  virtual bool Mitigate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerKeyLossMitigator);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_OWNER_KEY_LOSS_MITIGATOR_H_
