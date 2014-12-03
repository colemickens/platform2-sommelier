// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_SHILL_CLIENT_H_
#define PRIVETD_SHILL_CLIENT_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

#include "shill/dbus-proxies.h"

namespace privetd {

class ShillClient {
 public:
  // A callback that interested parties can register to be notified of
  // transitions from online to offline and vice versa.  The boolean
  // parameter will be true if we're online, and false if we're offline.
  using ConnectivityListener = base::Callback<void(bool)>;

  explicit ShillClient(const scoped_refptr<dbus::Bus>& bus);
  ~ShillClient() = default;

  void RegisterConnectivityListener(const ConnectivityListener& listener);
  // Causes shill to attempt to connect to the given network with the given
  // passphrase.  This is accomplished by:
  //  1) Configuring a service through the Manager with the SSID and passphrase.
  //  2) Calling Connect() on the service.
  //  2) Monitoring the returned Service object until we reach an online state,
  //     an error state, or another call to ConnectToService() occurs.
  // Returns false on immediate failures with some descriptive codes in |error|.
  bool ConnectToService(const std::string& ssid,
                        const std::string& passphrase,
                        const base::Closure& on_success,
                        chromeos::ErrorPtr* error);

 private:
  static bool IsConnectedState(const std::string& service_state);

  void OnServicePropertyChangeRegistration(const dbus::ObjectPath& path,
                                           const std::string& interface,
                                           const std::string& signal_name,
                                           bool success);
  void OnServicePropertyChange(const dbus::ObjectPath& service_path,
                               const std::string& property_name,
                               const chromeos::Any& property_value);
  // Clean up state related to a connecting service.  If
  // |check_for_reset_pending| is set, then we'll check to see if we've called
  // ConnectToService() in the time since a task to call this function was
  // posted.
  void CleanupConnectingService(bool check_for_reset_pending);

  std::vector<ConnectivityListener> connectivity_listeners_;
  const scoped_refptr<dbus::Bus> bus_;
  org::chromium::flimflam::ManagerProxy manager_proxy_;
  bool connecting_service_reset_pending_{false};
  bool have_called_connect_{false};
  std::unique_ptr<org::chromium::flimflam::ServiceProxy> connecting_service_;
  base::CancelableClosure on_connect_success_;
  base::WeakPtrFactory<ShillClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShillClient);
};

}  // namespace privetd

#endif  // PRIVETD_SHILL_CLIENT_H_
