// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/firewall_service.h"

#include "firewalld/dbus_interface.h"
#include "firewalld/iptables.h"

namespace firewalld {

FirewallService::FirewallService(const scoped_refptr<dbus::Bus>& bus)
    : org::chromium::FirewalldAdaptor(&iptables_),
      dbus_object_{nullptr, bus, dbus::ObjectPath{kFirewallServicePath}} {}

void FirewallService::RegisterAsync(const CompletionAction& callback) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(callback);
}

}  // namespace firewalld
