// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIREWALLD_FIREWALL_DAEMON_H_
#define FIREWALLD_FIREWALL_DAEMON_H_

#include <base/macros.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>

#include "firewalld/dbus_interface.h"
#include "firewalld/firewall_service.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace firewalld {

class FirewallDaemon : public chromeos::DBusServiceDaemon {
 public:
  FirewallDaemon();

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override;

 private:
  std::unique_ptr<FirewallService> firewall_service_;

  DISALLOW_COPY_AND_ASSIGN(FirewallDaemon);
};

}  // namespace firewalld

#endif  // FIREWALLD_FIREWALL_DAEMON_H_
