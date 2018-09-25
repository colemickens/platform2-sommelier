// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_MODEM_MANAGER_PROXY_H_
#define SHILL_DBUS_CHROMEOS_MODEM_MANAGER_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "cellular/dbus-proxies.h"
#include "shill/cellular/modem_manager_proxy_interface.h"

namespace shill {

class EventDispatcher;
class ModemManagerClassic;

// There's a single proxy per (old) ModemManager service identified by
// its DBus |path| and owner name |service|.
class ChromeosModemManagerProxy : public ModemManagerProxyInterface {
 public:
  ChromeosModemManagerProxy(
      EventDispatcher* dispatcher,
      const scoped_refptr<dbus::Bus>& bus,
      ModemManagerClassic* manager,
      const std::string& path,
      const std::string& service,
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback);
  ~ChromeosModemManagerProxy() override;

  // Inherited from ModemManagerProxyInterface.
  std::vector<std::string> EnumerateDevices() override;

 private:
  // Signal handlers.
  void DeviceAdded(const dbus::ObjectPath& device);
  void DeviceRemoved(const dbus::ObjectPath& device);

  // Called when service appeared or vanished.
  void OnServiceAvailable(bool available);

  // Service name owner changed handler.
  void OnServiceOwnerChanged(const std::string& old_owner,
                             const std::string& new_owner);

  // Called when signal is connected to the ObjectProxy.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  // Invoke |service_appeared_callback_| if it is set.
  void OnServiceAppeared();

  // Invoke |service_vanished_callback_| if it is set.
  void OnServiceVanished();

  std::unique_ptr<org::freedesktop::ModemManagerProxy> proxy_;
  EventDispatcher* dispatcher_;
  ModemManagerClassic* manager_;      // The owner of this proxy.
  base::Closure service_appeared_callback_;
  base::Closure service_vanished_callback_;
  bool service_available_;

  base::WeakPtrFactory<ChromeosModemManagerProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ChromeosModemManagerProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_MODEM_MANAGER_PROXY_H_
