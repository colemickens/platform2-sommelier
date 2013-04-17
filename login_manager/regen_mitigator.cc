// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/regen_mitigator.h"

#include "login_manager/policy_key.h"
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

bool RegenMitigator::Mitigate(const std::string& ownername) {
  return mitigating_ = generator_->Start(ownername, set_uid_ ? uid_ : 0);
}

bool RegenMitigator::Mitigating() {
  return mitigating_;
}

}  // namespace login_manager
