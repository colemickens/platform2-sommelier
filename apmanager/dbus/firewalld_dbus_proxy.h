// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DBUS_FIREWALLD_DBUS_PROXY_H_
#define APMANAGER_DBUS_FIREWALLD_DBUS_PROXY_H_

#include <string>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <firewalld/dbus-proxies.h>

#include "apmanager/firewall_proxy_interface.h"

namespace apmanager {

class EventDispatcher;

class FirewalldDBusProxy : public FirewallProxyInterface {
 public:
  FirewalldDBusProxy(const scoped_refptr<dbus::Bus>& bus,
                            const base::Closure& service_appeared_callback,
                            const base::Closure& service_vanished_callback);
  ~FirewalldDBusProxy() override;

  // Request/release access for an UDP port |port} on an interface |interface|.
  bool RequestUdpPortAccess(const std::string& interface,
                            uint16_t port) override;
  bool ReleaseUdpPortAccess(const std::string& interface,
                            uint16_t port) override;

 private:
  // Called when service appeared or vanished.
  void OnServiceAvailable(bool service_available);

  // Service name owner changed handler.
  void OnServiceOwnerChanged(const std::string& old_owner,
                            const std::string& new_owner);

  std::unique_ptr<org::chromium::FirewalldProxy> proxy_;
  EventDispatcher* dispatcher_;
  base::Closure service_appeared_callback_;
  base::Closure service_vanished_callback_;
  bool service_available_;

  base::WeakPtrFactory<FirewalldDBusProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FirewalldDBusProxy);
};

}  // namespace apmanager

#endif  // APMANAGER_DBUS_FIREWALLD_DBUS_PROXY_H_
