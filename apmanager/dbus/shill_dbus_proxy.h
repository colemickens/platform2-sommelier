// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DBUS_SHILL_DBUS_PROXY_H_
#define APMANAGER_DBUS_SHILL_DBUS_PROXY_H_

#include <string>

#include <base/macros.h>
#include <shill/dbus-proxies.h>

#include "apmanager/shill_proxy_interface.h"

namespace apmanager {

class EventDispatcher;

class ShillDBusProxy : public ShillProxyInterface {
 public:
  ShillDBusProxy(const scoped_refptr<dbus::Bus>& bus,
                 const base::Closure& service_appeared_callback,
                 const base::Closure& service_vanished_callback);
  ~ShillDBusProxy() override;

  // Implementation of ShillProxyInterface.
  bool ClaimInterface(const std::string& interface_name) override;
  bool ReleaseInterface(const std::string& interface_name) override;
#if defined(__BRILLO__)
  bool SetupApModeInterface(std::string* interface_name) override;
  bool SetupStationModeInterface(std::string* interface_name) override;
#endif  // __BRILLO__

 private:
  void OnServiceAvailable(bool service_available);
  void OnServiceOwnerChanged(const std::string& old_owner,
                             const std::string& new_owner);

  // DBus proxy for shill's manager interface.
  std::unique_ptr<org::chromium::flimflam::ManagerProxy> manager_proxy_;
  EventDispatcher* dispatcher_;
  base::Closure service_appeared_callback_;
  base::Closure service_vanished_callback_;
  bool service_available_;

  base::WeakPtrFactory<ShillDBusProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ShillDBusProxy);
};

}  // namespace apmanager

#endif  // APMANAGER_DBUS_SHILL_DBUS_PROXY_H_
