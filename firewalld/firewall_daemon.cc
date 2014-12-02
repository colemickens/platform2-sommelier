// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/firewall_daemon.h"

#include <string>

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

namespace firewalld {

void FirewallDaemon::RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) {
  firewall_service_.reset(new firewalld::FirewallService(bus_));
  firewall_service_->RegisterAsync(
      sequencer->GetHandler("Service.RegisterAsync() failed.", true));
}

}  // namespace firewalld
