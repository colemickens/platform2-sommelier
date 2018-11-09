// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/dbus/permission_broker_dbus_proxy.h"

#include <base/bind.h>
#include <brillo/errors/error.h>

#include "apmanager/event_dispatcher.h"

using std::string;

namespace apmanager {

PermissionBrokerDBusProxy::PermissionBrokerDBusProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback)
    : proxy_(new org::chromium::PermissionBrokerProxy(bus)),
      dispatcher_(EventDispatcher::GetInstance()),
      service_appeared_callback_(service_appeared_callback),
      service_vanished_callback_(service_vanished_callback),
      service_available_(false) {
  // Monitor service owner changes. This callback lives for the lifetime of
  // the ObjectProxy.
  proxy_->GetObjectProxy()->SetNameOwnerChangedCallback(
      base::Bind(&PermissionBrokerDBusProxy::OnServiceOwnerChanged,
                 weak_factory_.GetWeakPtr()));

  // One time callback when service becomes available.
  proxy_->GetObjectProxy()->WaitForServiceToBeAvailable(
      base::Bind(&PermissionBrokerDBusProxy::OnServiceAvailable,
                 weak_factory_.GetWeakPtr()));

  // Setup lifeline pipe to allow permission_broker to monitor this process,
  // so it can automatically remove the firewall rules when this process
  // crashed.
  int fds[2];
  CHECK(pipe(fds) == 0) << "Failed to create lifeline pipe";
  lifeline_read_fd_ = fds[0];
  lifeline_write_fd_ = fds[1];
}

PermissionBrokerDBusProxy::~PermissionBrokerDBusProxy() {
  close(lifeline_read_fd_);
  close(lifeline_write_fd_);
}

bool PermissionBrokerDBusProxy::RequestUdpPortAccess(const string& interface,
                                                     uint16_t port) {
  if (!service_available_) {
    LOG(ERROR) << "permission_broker service not available";
    return false;
  }

  // Pass the read end of the pipe to permission_broker, to allow it to
  // monitor on this process.
  bool allowed = false;
  brillo::ErrorPtr error;
  if (!proxy_->RequestUdpPortAccess(port, interface, lifeline_read_fd_,
                                    &allowed, &error)) {
    LOG(ERROR) << "Failed to request UDP port access: "
               << error->GetCode() << " " << error->GetMessage();
    return false;
  }
  if (!allowed) {
    LOG(ERROR) << "Access request for UDP port " << port
               << " on interface " << interface << " is denied";
    return false;
  }
  LOG(INFO) << "Access granted for UDP port " << port
            << " on interface " << interface;
  return true;
}

bool PermissionBrokerDBusProxy::ReleaseUdpPortAccess(const string& interface,
                                                     uint16_t port) {
  if (!service_available_) {
    LOG(ERROR) << "permission_broker service not available";
    return false;
  }

  brillo::ErrorPtr error;
  bool success;
  if (!proxy_->ReleaseUdpPort(port, interface, &success, &error)) {
    LOG(ERROR) << "Failed to release UDP port access: "
               << error->GetCode() << " " << error->GetMessage();
    return false;
  }
  if (!success) {
    LOG(ERROR) << "Release request for UDP port " << port
               << " on interface " << interface << " is denied";
    return false;
  }
  LOG(INFO) << "Access released for UDP port " << port
            << " on interface " << interface;
  return true;
}

void PermissionBrokerDBusProxy::OnServiceAvailable(bool available) {
  LOG(INFO) << __func__ << ": " << available;
  // The callback might invoke calls to the ObjectProxy, so defer the callback
  // to event loop.
  if (available && !service_appeared_callback_.is_null()) {
    dispatcher_->PostTask(service_appeared_callback_);
  } else if (!available && !service_vanished_callback_.is_null()) {
    dispatcher_->PostTask(service_vanished_callback_);
  }
  service_available_ = available;
}

void PermissionBrokerDBusProxy::OnServiceOwnerChanged(const string& old_owner,
                                                     const string& new_owner) {
  LOG(INFO) << __func__ << " old: " << old_owner << " new: " << new_owner;
  if (new_owner.empty()) {
    OnServiceAvailable(false);
  } else {
    OnServiceAvailable(true);
  }
}

}  // namespace apmanager
