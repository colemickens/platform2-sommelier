// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_CHARGE_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_CHARGE_CONTROLLER_H_

#include <string>

#include <base/macros.h>
#include <base/optional.h>

#include "power_manager/proto_bindings/policy.pb.h"

namespace power_manager {

namespace system {
class ChargeControllerHelperInterface;
}  // namespace system

namespace policy {

// ChargeController is responsible for handling power policies:
// peak shift, advanced battery charge.
class ChargeController {
 public:
  ChargeController();
  ~ChargeController();

  // |helper| must be non-null.
  void Init(system::ChargeControllerHelperInterface* helper);

  // Does nothing if |policy| and |cached_policy_| peak shift related fields are
  // equal. Otherwise updates the charging configuration per |policy| and
  // copies |policy| to |cached_policy_|.
  void HandlePolicyChange(const PowerManagementPolicy& policy);

 private:
  bool ApplyPolicyChange(const PowerManagementPolicy& policy);

  // Calls delegate's |SetPeakShiftDayConfig| function and returns the result
  // if |day_config| contains all needed fields, otherwise returns false.
  bool SetPeakShiftDayConfig(
      const PowerManagementPolicy::PeakShiftDayConfig& day_config);

  // Checks that charging-related fields are equal between |policy| and
  // |cached_policy_|.
  bool IsPolicyEqualToCache(const PowerManagementPolicy& policy) const;

  // Not owned.
  system::ChargeControllerHelperInterface* helper_;

  // Contains last successfully applied power policies settings.
  base::Optional<PowerManagementPolicy> cached_policy_;

  DISALLOW_COPY_AND_ASSIGN(ChargeController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_CHARGE_CONTROLLER_H_
