// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/firewall_service.h"

#include "firewalld/dbus_interface.h"
#include "firewalld/iptables.h"

namespace firewalld {

FirewallService::FirewallService(const scoped_refptr<dbus::Bus>& bus)
    : org::chromium::FirewalldAdaptor(&iptables_),
      dbus_object_{nullptr, bus, dbus::ObjectPath{kFirewallServicePath}},
      weak_ptr_factory_{this} {}

void FirewallService::RegisterAsync(const CompletionAction& callback) {
  RegisterWithDBusObject(&dbus_object_);

  // Track permission_broker's lifetime so that we can close firewall holes
  // if/when permission_broker exits.
  permission_broker_.reset(
      new org::chromium::PermissionBroker::ObjectManagerProxy(
          dbus_object_.GetBus()));
  permission_broker_->SetPermissionBrokerRemovedCallback(
      base::Bind(&FirewallService::OnPermissionBrokerRemoved,
                 weak_ptr_factory_.GetWeakPtr()));

  dbus_object_.RegisterAsync(callback);
}

void FirewallService::OnPermissionBrokerRemoved(const dbus::ObjectPath& path) {
  LOG(INFO) << "permission_broker died, plugging all firewall holes";
  iptables_.PlugAllHoles();
}

}  // namespace firewalld
