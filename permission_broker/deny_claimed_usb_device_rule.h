// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_DENY_CLAIMED_USB_DEVICE_RULE_H_
#define PERMISSION_BROKER_DENY_CLAIMED_USB_DEVICE_RULE_H_

#include <string>

#include "base/basictypes.h"
#include "permission_broker/rule.h"

struct udev;

namespace permission_broker {

// DenyClaimedUsbDeviceRule encapsulates the policy that any USB device that is
// claimed by a driver is |DENY|'d, while all other requests are |IGNORE|d.  It
// does this by walking the udev device tree (the entire tree, not just the USB
// subsystem) and attempts, for each device entry, to find a parent device
// within the USB subsystem whose device node property is the same as the |path|
// parameter. If such a matching device exists, the path is rejected as it has
// been demonstrated to be claimed by another udev entry.
class DenyClaimedUsbDeviceRule : public Rule {
 public:
  DenyClaimedUsbDeviceRule();
  virtual ~DenyClaimedUsbDeviceRule();

  virtual Result Process(const std::string &path);

 private:
  struct udev *const udev_;

  DISALLOW_COPY_AND_ASSIGN(DenyClaimedUsbDeviceRule);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_CLAIMED_USB_DEVICE_RULE_H_
