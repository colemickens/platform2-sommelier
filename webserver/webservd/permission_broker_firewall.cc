// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/webservd/permission_broker_firewall.h"

#include <unistd.h>

#include <string>

#include <base/bind.h>
#include <base/macros.h>

namespace webservd {

PermissionBrokerFirewall::PermissionBrokerFirewall() {
  int fds[2];
  PCHECK(pipe(fds) == 0) << "Failed to create firewall lifeline pipe";
  lifeline_read_fd_ = fds[0];
  lifeline_write_fd_ = fds[1];
}

PermissionBrokerFirewall::~PermissionBrokerFirewall() {
  close(lifeline_read_fd_);
  close(lifeline_write_fd_);
}

void PermissionBrokerFirewall::WaitForServiceAsync(
    dbus::Bus* bus,
    const base::Closure& callback) {
  service_online_cb_ = callback;
  object_manager_.reset(
      new org::chromium::PermissionBroker::ObjectManagerProxy{bus});
  object_manager_->SetPermissionBrokerAddedCallback(
      base::Bind(&PermissionBrokerFirewall::OnPermissionBrokerOnline,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PermissionBrokerFirewall::PunchTcpHoleAsync(
    uint16_t port,
    const std::string& interface_name,
    const base::Callback<void(bool)>& success_cb,
    const base::Callback<void(chromeos::Error*)>& failure_cb) {
  dbus::FileDescriptor dbus_fd{lifeline_read_fd_};
  dbus_fd.CheckValidity();
  proxy_->RequestTcpPortAccessAsync(port, interface_name, dbus_fd, success_cb,
                                    failure_cb);
}

void PermissionBrokerFirewall::OnPermissionBrokerOnline(
    org::chromium::PermissionBrokerProxy* proxy) {
  proxy_ = proxy;
  service_online_cb_.Run();
}

}  // namespace webservd
