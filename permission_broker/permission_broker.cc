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
const int kMaxEvents = 10;
const int64 kLifelineIntervalSeconds = 5;
}

namespace permission_broker {

PermissionBroker::PermissionBroker(
    const scoped_refptr<dbus::Bus>& bus,
    const std::string& access_group_name,
    const std::string& udev_run_path,
    int poll_interval_msecs)
    : org::chromium::PermissionBrokerAdaptor(this),
      task_runner_(base::MessageLoopForIO::current()->task_runner()),
      epfd_(-1),
      rule_engine_(access_group_name, udev_run_path, poll_interval_msecs),
      dbus_object_(nullptr,
                   bus,
                   dbus::ObjectPath(kPermissionBrokerServicePath)),
      firewalld_(bus, firewalld::kServiceName) {
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

PermissionBroker::~PermissionBroker() {
  if (epfd_ >= 0) {
    close(epfd_);
  }
}

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
  // We use |in_lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(in_lifeline_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for TCP port " << in_port << " failed";
    return false;
  }

  // Track the port we just requested.
  tcp_ports_[lifeline_fd] = in_port;

  bool success;
  firewalld_.PunchTcpHole(in_port, &success, nullptr);
  if (!success) {
    // If we fail to punch the hole in the firewall, stop tracking the lifetime
    // of the process.
    LOG(ERROR) << "Failed to punch hole for TCP port " << in_port;
    DeleteLifelineFd(lifeline_fd);
    tcp_ports_.erase(lifeline_fd);
    return false;
  }
  return true;
}

bool PermissionBroker::RequestUdpPortAccess(
    uint16_t in_port,
    const dbus::FileDescriptor& in_lifeline_fd) {
  // We use |in_lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(in_lifeline_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for UDP port " << in_port << " failed";
    return false;
  }

  // Track the port we just requested.
  udp_ports_[lifeline_fd] = in_port;

  bool success;
  firewalld_.PunchUdpHole(in_port, &success, nullptr);
  if (!success) {
    // If we fail to punch the hole in the firewall, stop tracking the lifetime
    // of the process.
    LOG(ERROR) << "Failed to punch hole for UDP port " << in_port;
    DeleteLifelineFd(lifeline_fd);
    udp_ports_.erase(lifeline_fd);
    return false;
  }
  return true;
}

int PermissionBroker::AddLifelineFd(const dbus::FileDescriptor& dbus_fd) {
  if (!InitializeEpollOnce()) {
    return -1;
  }
  int fd = dup(dbus_fd.value());

  struct epoll_event epevent;
  epevent.events =
      EPOLLIN | EPOLLET;  // EPOLLERR | EPOLLHUP are always waited for.
  epevent.data.fd = fd;
  LOG(INFO) << "Adding file descriptor " << fd << " to epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &epevent) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_ADD)";
    return -1;
  }

  // If this is the first port request, start lifeline checks.
  if (tcp_ports_.size() + udp_ports_.size() == 0) {
    LOG(INFO) << "Starting lifeline checks";
    ScheduleLifelineCheck();
  }

  return fd;
}

bool PermissionBroker::DeleteLifelineFd(int fd) {
  if (epfd_ < 0) {
    LOG(ERROR) << "epoll instance not created";
    return false;
  }

  LOG(INFO) << "Deleting file descriptor " << fd << " from epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_DEL)";
    return false;
  }
  return true;
}

void PermissionBroker::CheckLifelineFds() {
  if (epfd_ < 0) {
    return;
  }
  struct epoll_event epevents[kMaxEvents];
  int nready = epoll_wait(epfd_, epevents, kMaxEvents, 0 /* do not block */);
  if (nready < 0) {
    PLOG(ERROR) << "epoll_wait(0)";
    return;
  }
  if (nready == 0) {
    ScheduleLifelineCheck();
    return;
  }

  for (size_t eidx = 0; eidx < (size_t)nready; eidx++) {
    uint32_t events = epevents[eidx].events;
    int fd = epevents[eidx].data.fd;
    if ((events & (EPOLLHUP | EPOLLERR))) {
      // The process that requested this port has died/exited,
      // so we need to plug the hole.
      PlugFirewallHole(fd);
      DeleteLifelineFd(fd);
    }
  }

  // If there's still processes to track, schedule lifeline checks.
  if (tcp_ports_.size() + udp_ports_.size() > 0) {
    ScheduleLifelineCheck();
  } else {
    LOG(INFO) << "Stopping lifeline checks";
  }
}

void PermissionBroker::ScheduleLifelineCheck() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PermissionBroker::CheckLifelineFds, base::Unretained(this)),
      base::TimeDelta::FromSeconds(kLifelineIntervalSeconds));
}

void PermissionBroker::PlugFirewallHole(int fd) {
  bool success = false;
  uint16_t port = 0;
  if (tcp_ports_.find(fd) != tcp_ports_.end()) {
    // It was a TCP port.
    port = tcp_ports_[fd];
    firewalld_.PlugTcpHole(port, &success, nullptr);
    tcp_ports_.erase(fd);
    if (!success) {
      LOG(ERROR) << "Failed to plug hole for TCP port " << port;
    }
  } else if (udp_ports_.find(fd) != udp_ports_.end()) {
    // It was a UDP port.
    port = udp_ports_[fd];
    firewalld_.PlugUdpHole(port, &success, nullptr);
    udp_ports_.erase(fd);
    if (!success) {
      LOG(ERROR) << "Failed to plug hole for UDP port " << port;
    }
  } else {
    LOG(ERROR) << "File descriptor " << fd << " was not being tracked";
  }
}

bool PermissionBroker::InitializeEpollOnce() {
  if (epfd_ < 0) {
    // |size| needs to be > 0, but is otherwise ignored.
    LOG(INFO) << "Creating epoll instance";
    epfd_ = epoll_create(1 /* size */);
    if (epfd_ < 0) {
      PLOG(ERROR) << "epoll_create()";
      return false;
    }
  }
  return true;
}

}  // namespace permission_broker
