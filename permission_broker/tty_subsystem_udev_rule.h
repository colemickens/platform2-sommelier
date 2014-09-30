// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_TTY_SUBSYSTEM_UDEV_RULE_H_
#define PERMISSION_BROKER_TTY_SUBSYSTEM_UDEV_RULE_H_

#include <string>

#include "permission_broker/udev_rule.h"

namespace permission_broker {

// TtySubsystemUdevRule is a UdevRule that calls ProcessTtyDevice on every
// device that belongs to the TTY subsystem. All other non-TTY devices are
// ignored by this rule.
class TtySubsystemUdevRule : public UdevRule {
 public:
  explicit TtySubsystemUdevRule(const std::string& name);
  ~TtySubsystemUdevRule() override = default;

  // Called with every device belonging to the TTY subsystem. The return value
  // from ProcessTtyDevice is returned directly as the result of processing this
  // rule.
  virtual Result ProcessTtyDevice(udev_device* device) = 0;

  Result ProcessDevice(udev_device* device) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TtySubsystemUdevRule);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_TTY_SUBSYSTEM_UDEV_RULE_H_
