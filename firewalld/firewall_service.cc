// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/firewall_service.h"

#include "firewalld/dbus_interface.h"

namespace firewalld {

FirewallService::FirewallService(const scoped_refptr<dbus::Bus>& bus)
    : org::chromium::FirewalldAdaptor(this),
      dbus_object_{nullptr, bus, dbus::ObjectPath{kFirewallServicePath}} {}

void FirewallService::RegisterAsync(const CompletionAction& callback) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(callback);
}

bool FirewallService::PunchHole(chromeos::ErrorPtr* error,
                                uint16_t in_port,
                                bool* out_success) {
  *out_success = false;

  if (in_port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    *out_success = false;
    return true;
  }

  // TODO(jorgelo): implement this.
  LOG(ERROR) << "Punching hole for port " << in_port;
  *out_success = true;
  return true;
}

bool FirewallService::PlugHole(chromeos::ErrorPtr* error,
                               uint16_t in_port,
                               bool* out_success) {
  *out_success = false;

  if (in_port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    *out_success = false;
    return true;
  }

  // TODO(jorgelo): implement this.
  LOG(ERROR) << "Plugging hole for port " << in_port;
  *out_success = true;
  return true;
}

}  // namespace firewalld
