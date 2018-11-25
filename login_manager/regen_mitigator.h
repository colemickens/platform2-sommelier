// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_REGEN_MITIGATOR_H_
#define LOGIN_MANAGER_REGEN_MITIGATOR_H_

#include <string>

#include <base/macros.h>

#include "login_manager/owner_key_loss_mitigator.h"

namespace login_manager {

class KeyGenerator;
class PolicyKey;

// This class mitigates owner key loss by triggering the generation of a new
// owner key and the re-signing of exisiting owner device policy.
class RegenMitigator : public OwnerKeyLossMitigator {
 public:
  explicit RegenMitigator(KeyGenerator* generator);
  ~RegenMitigator() override;

  // Deal with loss of the owner's private key.
  // Returning true means that we can recover without user interaction.
  // Returning false means that we can't.
  bool Mitigate(const std::string& ownername) override;
  bool Mitigating() override;

 private:
  KeyGenerator* generator_;
  bool mitigating_ = false;
  DISALLOW_COPY_AND_ASSIGN(RegenMitigator);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_REGEN_MITIGATOR_H_
