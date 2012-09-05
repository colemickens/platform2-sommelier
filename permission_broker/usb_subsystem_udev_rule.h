// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_USB_SUBSYSTEM_UDEV_RULE_H_
#define PERMISSION_BROKER_USB_SUBSYSTEM_UDEV_RULE_H_

#include <string>

#include "permission_broker/udev_rule.h"

namespace permission_broker {

// UsbSubsystemUdevRule is a UdevRule that calls ProcessUsbDevice on every
// device that belongs to the USB subsystem. All other non-USB devices are
// ignored by this rule.
class UsbSubsystemUdevRule : public UdevRule {
 public:
  UsbSubsystemUdevRule(const std::string &name);
  virtual ~UsbSubsystemUdevRule();

  // Called with every device belonging to the USB subsystem. The return value
  // from ProcessUsbDevice is returned directly as the result of processing this
  // rule.
  virtual Result ProcessUsbDevice(struct udev_device *device) = 0;

  virtual Result ProcessDevice(struct udev_device *device);

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbSubsystemUdevRule);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_USB_SUBSYSTEM_UDEV_RULE_H_
