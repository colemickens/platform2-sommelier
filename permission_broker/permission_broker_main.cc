// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <glib-object.h>
#include <linux/usb/ch9.h>

#include "base/logging.h"
#include "permission_broker/allow_usb_device_rule.h"
#include "permission_broker/deny_claimed_usb_device_rule.h"
#include "permission_broker/deny_uninitialized_device_rule.h"
#include "permission_broker/deny_usb_device_class_rule.h"
#include "permission_broker/deny_usb_vendor_id_rule.h"
#include "permission_broker/permission_broker.h"

using permission_broker::AllowUsbDeviceRule;
using permission_broker::DenyClaimedUsbDeviceRule;
using permission_broker::DenyUninitializedDeviceRule;
using permission_broker::DenyUsbDeviceClassRule;
using permission_broker::DenyUsbVendorIdRule;
using permission_broker::PermissionBroker;

static const uint16_t kLinuxFoundationUsbVendorId = 0x1d6b;

int main(int argc, char **argv) {
  g_type_init();
  google::ParseCommandLineFlags(&argc, &argv, true);

  PermissionBroker broker;
  broker.AddRule(new AllowUsbDeviceRule());
  broker.AddRule(new DenyClaimedUsbDeviceRule());
  broker.AddRule(new DenyUninitializedDeviceRule());
  broker.AddRule(new DenyUsbDeviceClassRule(USB_CLASS_HUB));
  broker.AddRule(new DenyUsbDeviceClassRule(USB_CLASS_MASS_STORAGE));
  broker.AddRule(new DenyUsbVendorIdRule(kLinuxFoundationUsbVendorId));
  broker.Run();

  return 0;
}
