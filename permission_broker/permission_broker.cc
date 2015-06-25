// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/permission_broker.h"

#include <fcntl.h>
#include <grp.h>
#include <linux/usb/ch9.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
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

const char kErrorDomainPermissionBroker[] = "permission_broker";
const char kPermissionDeniedError[] = "permission_denied";
const char kOpenFailedError[] = "open_failed";
}

namespace permission_broker {

PermissionBroker::PermissionBroker(
    chromeos::dbus_utils::ExportedObjectManager* object_manager,
    const std::string& access_group_name,
    const std::string& udev_run_path,
    int poll_interval_msecs)
    : org::chromium::PermissionBrokerAdaptor(this),
      rule_engine_(udev_run_path, poll_interval_msecs),
      dbus_object_(object_manager,
                   object_manager->GetBus(),
                   dbus::ObjectPath(kPermissionBrokerServicePath)),
      // Create the FirewalldProxy object here, that way the PortTracker object
      // doesn't need to know about D-Bus, which makes testing easier.
      firewalld_(object_manager->GetBus(), firewalld::kServiceName),
      // |firewalld_| is owned by PermissionBroker, the PortTracker object
      // will only call D-Bus methods.
      port_tracker_(&firewalld_) {
  CHECK(!access_group_name.empty()) << "You must specify a group name via the "
                                    << "--access_group flag.";
  struct group group_buffer;
  struct group* access_group = NULL;
  char buffer[256];
  getgrnam_r(access_group_name.c_str(), &group_buffer, buffer, sizeof(buffer),
             &access_group);
  CHECK(access_group) << "Could not resolve \"" << access_group_name << "\" "
                      << "to a named group.";
  access_group_ = access_group->gr_gid;

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

bool PermissionBroker::CheckPathAccess(const std::string& in_path) {
  return rule_engine_.ProcessPath(in_path, Rule::ANY_INTERFACE);
}

bool PermissionBroker::RequestPathAccess(const std::string& in_path,
                                         int32_t in_interface_id) {
  if (rule_engine_.ProcessPath(in_path, in_interface_id)) {
    return GrantAccess(in_path);
  }
  return false;
}

bool PermissionBroker::OpenPath(chromeos::ErrorPtr* error,
                                const std::string& in_path,
                                dbus::FileDescriptor* out_fd) {
  if (!rule_engine_.ProcessPath(in_path, Rule::ANY_INTERFACE)) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, kErrorDomainPermissionBroker, kPermissionDeniedError,
        "Permission to open '%s' denied", in_path.c_str());
    return false;
  }

  int fd = HANDLE_EINTR(open(in_path.c_str(), O_RDWR));
  if (fd < 0) {
    chromeos::errors::system::AddSystemError(error, FROM_HERE, errno);
    chromeos::Error::AddToPrintf(error, FROM_HERE, kErrorDomainPermissionBroker,
                                 kOpenFailedError, "Failed to open path '%s'",
                                 in_path.c_str());
    return false;
  }

  dbus::FileDescriptor result;
  result.PutValue(fd);
  result.CheckValidity();

  *out_fd = result.Pass();
  return true;
}

bool PermissionBroker::RequestTcpPortAccess(
    uint16_t in_port,
    const std::string& in_interface,
    const dbus::FileDescriptor& in_lifeline_fd) {
  return port_tracker_.ProcessTcpPort(in_port, in_interface,
                                      in_lifeline_fd.value());
}

bool PermissionBroker::RequestUdpPortAccess(
    uint16_t in_port,
    const std::string& in_interface,
    const dbus::FileDescriptor& in_lifeline_fd) {
  return port_tracker_.ProcessUdpPort(in_port, in_interface,
                                      in_lifeline_fd.value());
}

bool PermissionBroker::ReleaseTcpPort(uint16_t in_port,
                                      const std::string& in_interface) {
  return port_tracker_.ReleaseTcpPort(in_port, in_interface);
}

bool PermissionBroker::ReleaseUdpPort(uint16_t in_port,
                                      const std::string& in_interface) {
  return port_tracker_.ReleaseUdpPort(in_port, in_interface);
}

bool PermissionBroker::RequestVpnSetup(
    const std::vector<std::string>& usernames,
    const std::string& interface,
    const dbus::FileDescriptor& in_lifeline_fd) {
  return port_tracker_.ProcessVpnSetup(usernames,
                                       interface,
                                       in_lifeline_fd.value());
}

bool PermissionBroker::RemoveVpnSetup() {
  return port_tracker_.RemoveVpnSetup();
}

bool PermissionBroker::GrantAccess(const std::string& path) {
  if (chown(path.c_str(), -1, access_group_)) {
    PLOG(INFO) << "Could not grant access to " << path;
    return false;
  }
  return true;
}

}  // namespace permission_broker
