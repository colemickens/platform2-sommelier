// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/regen_mitigator.h"

#include "login_manager/owner_key.h"
#include "login_manager/session_manager_service.h"

namespace login_manager {

RegenMitigator::RegenMitigator(KeyGenerator* generator,
                               bool set_uid,
                               uid_t uid,
                               SessionManagerService* manager)
    : generator_(generator),
      set_uid_(set_uid),
      uid_(uid),
      manager_(manager),
      mitigating_(false) {
  DCHECK(generator_);
}

RegenMitigator::~RegenMitigator() {}

bool RegenMitigator::Mitigate(OwnerKey* key) {
  return mitigating_ = generator_->Start(set_uid_ ? uid_ : 0, manager_);
}

bool RegenMitigator::Mitigating() {
  return mitigating_;
}

}  // namespace login_manager
