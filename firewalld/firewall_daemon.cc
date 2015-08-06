// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/firewall_daemon.h"

#include <string>

#include <base/logging.h>

namespace firewalld {

FirewallDaemon::FirewallDaemon()
    : chromeos::DBusServiceDaemon{kFirewallServiceName,
                                  dbus::ObjectPath{kFirewallServicePath}} {
}

void FirewallDaemon::RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) {
  firewall_service_.reset(
      new firewalld::FirewallService{object_manager_.get()});
  firewall_service_->RegisterAsync(
      sequencer->GetHandler("Service.RegisterAsync() failed.", true));
}

}  // namespace firewalld
