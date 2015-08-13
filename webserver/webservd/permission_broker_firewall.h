// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_PERMISSION_BROKER_FIREWALL_H_
#define WEBSERVER_WEBSERVD_PERMISSION_BROKER_FIREWALL_H_

#include "webserver/webservd/firewall_interface.h"

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "permission_broker/dbus-proxies.h"

namespace webservd {

class PermissionBrokerFirewall : public FirewallInterface {
 public:
  PermissionBrokerFirewall();
  ~PermissionBrokerFirewall() override;

  // Interface overrides.
  void WaitForServiceAsync(dbus::Bus* bus,
                           const base::Closure& callback) override;
  void PunchTcpHoleAsync(
      uint16_t port, const std::string& interface_name,
      const base::Callback<void(bool)>& success_cb,
      const base::Callback<void(chromeos::Error*)>& failure_cb) override;

 private:
  void OnPermissionBrokerOnline(org::chromium::PermissionBrokerProxy* proxy);

  std::unique_ptr<org::chromium::PermissionBroker::ObjectManagerProxy>
      object_manager_;

  // Proxy to the firewall DBus service. Owned by the DBus bindings module.
  org::chromium::PermissionBrokerProxy* proxy_;

  // Callback to use when firewall service comes online.
  base::Closure service_online_cb_;

  // File descriptors for the two ends of the pipe used for communicating with
  // remote firewall server (permission_broker), where the remote firewall
  // server will use the read end of the pipe to detect when this process exits.
  int lifeline_read_fd_{-1};
  int lifeline_write_fd_{-1};

  base::WeakPtrFactory<PermissionBrokerFirewall> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PermissionBrokerFirewall);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_PERMISSION_BROKER_FIREWALL_H_
