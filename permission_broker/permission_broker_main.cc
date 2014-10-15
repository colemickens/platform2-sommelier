// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>
#include <glib-object.h>
#include <linux/usb/ch9.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "permission_broker/allow_group_tty_device_rule.h"
#include "permission_broker/allow_hidraw_device_rule.h"
#include "permission_broker/allow_tty_device_rule.h"
#include "permission_broker/allow_usb_device_rule.h"
#include "permission_broker/deny_claimed_hidraw_device_rule.h"
#include "permission_broker/deny_claimed_usb_device_rule.h"
#include "permission_broker/deny_group_tty_device_rule.h"
#include "permission_broker/deny_uninitialized_device_rule.h"
#include "permission_broker/deny_unsafe_hidraw_device_rule.h"
#include "permission_broker/deny_usb_device_class_rule.h"
#include "permission_broker/deny_usb_vendor_id_rule.h"
#include "permission_broker/permission_broker.h"

using permission_broker::AllowGroupTtyDeviceRule;
using permission_broker::AllowHidrawDeviceRule;
using permission_broker::AllowTtyDeviceRule;
using permission_broker::AllowUsbDeviceRule;
using permission_broker::DenyClaimedHidrawDeviceRule;
using permission_broker::DenyClaimedUsbDeviceRule;
using permission_broker::DenyGroupTtyDeviceRule;
using permission_broker::DenyUninitializedDeviceRule;
using permission_broker::DenyUnsafeHidrawDeviceRule;
using permission_broker::DenyUsbDeviceClassRule;
using permission_broker::DenyUsbVendorIdRule;
using permission_broker::PermissionBroker;

static const uint16_t kLinuxFoundationUsbVendorId = 0x1d6b;

int main(int argc, char **argv) {
  DEFINE_string(access_group, "", "The group which has resource access granted "
                "to it. Must not be empty.");
  DEFINE_int32(poll_interval, 100, "The interval at which to poll for udev "
               "events.");
  DEFINE_string(udev_run_path, "/run/udev",
                "The path to udev's run directory.");

  g_type_init();
  chromeos::FlagHelper::Init(argc, argv, "Chromium OS Permission Broker");
  chromeos::InitLog(chromeos::kLogToSyslog);

  PermissionBroker broker(FLAGS_access_group,
                          FLAGS_udev_run_path,
                          FLAGS_poll_interval);
  broker.AddRule(new AllowUsbDeviceRule());
  broker.AddRule(new AllowTtyDeviceRule());
  broker.AddRule(new DenyClaimedUsbDeviceRule());
  broker.AddRule(new DenyUninitializedDeviceRule());
  broker.AddRule(new DenyUsbDeviceClassRule(USB_CLASS_HUB));
  broker.AddRule(new DenyUsbDeviceClassRule(USB_CLASS_MASS_STORAGE));
  broker.AddRule(new DenyUsbVendorIdRule(kLinuxFoundationUsbVendorId));
  broker.AddRule(new AllowHidrawDeviceRule());
  broker.AddRule(new AllowGroupTtyDeviceRule("serial"));
  broker.AddRule(new DenyGroupTtyDeviceRule("modem"));
  broker.AddRule(new DenyGroupTtyDeviceRule("tty"));
  broker.AddRule(new DenyGroupTtyDeviceRule("uucp"));
  broker.AddRule(new DenyClaimedHidrawDeviceRule());
  broker.AddRule(new DenyUnsafeHidrawDeviceRule());
  broker.Run();

  return 0;
}
