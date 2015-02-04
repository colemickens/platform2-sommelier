// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/permission_broker.h"

#include <linux/usb/ch9.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

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
#include "permission_broker/rule.h"

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

namespace {
const uint16_t kLinuxFoundationUsbVendorId = 0x1d6b;
}

namespace permission_broker {

PermissionBroker::PermissionBroker(const scoped_refptr<dbus::Bus>& bus,
                                   const std::string& access_group_name,
                                   const std::string& udev_run_path,
                                   int poll_interval_msecs)
    : org::chromium::PermissionBrokerAdaptor(this),
      rule_engine_(access_group_name, udev_run_path, poll_interval_msecs),
      dbus_object_(nullptr,
                   bus,
                   dbus::ObjectPath(kPermissionBrokerServicePath)),
      // Create the FirewalldProxy object here, that way the PortTracker object
      // doesn't need to know about D-Bus, which makes testing easier.
      firewalld_(bus, firewalld::kServiceName),
      // |firewalld_| is owned by PermissionBroker, the PortTracker object
      // will only call D-Bus methods.
      port_tracker_(&firewalld_) {
  rule_engine_.AddRule(new AllowUsbDeviceRule());
  rule_engine_.AddRule(new AllowTtyDeviceRule());
  rule_engine_.AddRule(new DenyClaimedUsbDeviceRule());
  rule_engine_.AddRule(new DenyUninitializedDeviceRule());
  rule_engine_.AddRule(new DenyUsbDeviceClassRule(USB_CLASS_HUB));
  rule_engine_.AddRule(new DenyUsbDeviceClassRule(USB_CLASS_MASS_STORAGE));
  rule_engine_.AddRule(new DenyUsbVendorIdRule(kLinuxFoundationUsbVendorId));
  rule_engine_.AddRule(new AllowHidrawDeviceRule());
  rule_engine_.AddRule(new AllowGroupTtyDeviceRule("serial"));
  rule_engine_.AddRule(new DenyGroupTtyDeviceRule("modem"));
  rule_engine_.AddRule(new DenyGroupTtyDeviceRule("tty"));
  rule_engine_.AddRule(new DenyGroupTtyDeviceRule("uucp"));
  rule_engine_.AddRule(new DenyClaimedHidrawDeviceRule());
  rule_engine_.AddRule(new DenyUnsafeHidrawDeviceRule());
}

PermissionBroker::~PermissionBroker() {}

void PermissionBroker::RegisterAsync(
    const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction& cb) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(cb);
}

bool PermissionBroker::RequestPathAccess(const std::string& in_path,
                                         int32_t in_interface_id) {
  return rule_engine_.ProcessPath(in_path, in_interface_id);
}

bool PermissionBroker::RequestTcpPortAccess(
    uint16_t in_port,
    const dbus::FileDescriptor& in_lifeline_fd) {
  return port_tracker_.ProcessTcpPort(in_port, in_lifeline_fd.value());
}

bool PermissionBroker::RequestUdpPortAccess(
    uint16_t in_port,
    const dbus::FileDescriptor& in_lifeline_fd) {
  return port_tracker_.ProcessUdpPort(in_port, in_lifeline_fd.value());
}

}  // namespace permission_broker
