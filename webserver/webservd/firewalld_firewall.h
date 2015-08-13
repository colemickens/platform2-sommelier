// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_FIREWALLD_FIREWALL_H_
#define WEBSERVER_WEBSERVD_FIREWALLD_FIREWALL_H_

#include "webserver/webservd/firewall_interface.h"

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "firewalld/dbus-proxies.h"

namespace webservd {

class FirewalldFirewall : public FirewallInterface {
 public:
  FirewalldFirewall() = default;
  ~FirewalldFirewall() override = default;

  // Interface overrides.
  void WaitForServiceAsync(dbus::Bus* bus, const base::Closure& callback)
      override;
  void PunchTcpHoleAsync(
      uint16_t port, const std::string& interface_name,
      const base::Callback<void(bool)>& success_cb,
      const base::Callback<void(chromeos::Error*)>& failure_cb) override;

 private:
  void OnFirewalldOnline(org::chromium::FirewalldProxy* proxy);

  std::unique_ptr<org::chromium::Firewalld::ObjectManagerProxy> object_manager_;

  // Proxy to the firewall DBus service. Owned by the DBus bindings module.
  org::chromium::FirewalldProxy* proxy_;

  // Callback to use when firewall service comes online.
  base::Closure service_online_cb_;

  base::WeakPtrFactory<FirewalldFirewall> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FirewalldFirewall);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_FIREWALLD_FIREWALL_H_
