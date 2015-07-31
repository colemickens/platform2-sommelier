// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_SHILL_CLIENT_H_
#define LIBWEAVE_SRC_PRIVET_SHILL_CLIENT_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

#include "shill/dbus-proxies.h"
#include "weave/network.h"

namespace weave {
namespace privet {

class ShillClient final : public Network {
 public:
  ShillClient(const scoped_refptr<dbus::Bus>& bus,
              const std::set<std::string>& device_whitelist);
  ~ShillClient() = default;

  void Init();

  // Network implementation.
  void AddOnConnectionChangedCallback(
      const OnConnectionChangedCallback& listener) override;
  bool ConnectToService(const std::string& ssid,
                        const std::string& passphrase,
                        const base::Closure& on_success,
                        chromeos::ErrorPtr* error) override;
  NetworkState GetConnectionState() const override;

 private:
  struct DeviceState {
    std::unique_ptr<org::chromium::flimflam::DeviceProxy> device;
    // ServiceProxy objects are shared because the connecting service will
    // also be the selected service for a device, but is not always the selected
    // service (for instance, in the period between configuring a WiFi service
    // with credentials, and when Connect() is called.)
    std::shared_ptr<org::chromium::flimflam::ServiceProxy> selected_service;
    NetworkState service_state{NetworkState::kOffline};
  };

  bool IsMonitoredDevice(org::chromium::flimflam::DeviceProxy* device);
  void OnShillServiceOwnerChange(const std::string& old_owner,
                                 const std::string& new_owner);
  void OnManagerPropertyChangeRegistration(const std::string& interface,
                                           const std::string& signal_name,
                                           bool success);
  void OnManagerPropertyChange(const std::string& property_name,
                               const chromeos::Any& property_value);
  void OnDevicePropertyChangeRegistration(const dbus::ObjectPath& device_path,
                                          const std::string& interface,
                                          const std::string& signal_name,
                                          bool success);
  void OnDevicePropertyChange(const dbus::ObjectPath& device_path,
                              const std::string& property_name,
                              const chromeos::Any& property_value);
  void OnServicePropertyChangeRegistration(const dbus::ObjectPath& path,
                                           const std::string& interface,
                                           const std::string& signal_name,
                                           bool success);
  void OnServicePropertyChange(const dbus::ObjectPath& service_path,
                               const std::string& property_name,
                               const chromeos::Any& property_value);

  void OnStateChangeForConnectingService(const dbus::ObjectPath& service_path,
                                         const std::string& state);
  void OnStrengthChangeForConnectingService(
      const dbus::ObjectPath& service_path,
      uint8_t signal_strength);
  void OnStateChangeForSelectedService(const dbus::ObjectPath& service_path,
                                       const std::string& state);
  void UpdateConnectivityState();
  void NotifyConnectivityListeners(bool am_online);
  // Clean up state related to a connecting service.  If
  // |check_for_reset_pending| is set, then we'll check to see if we've called
  // ConnectToService() in the time since a task to call this function was
  // posted.
  void CleanupConnectingService(bool check_for_reset_pending);

  const scoped_refptr<dbus::Bus> bus_;
  org::chromium::flimflam::ManagerProxy manager_proxy_;
  // There is logic that assumes we will never change this device list
  // in OnManagerPropertyChange.  Do not be tempted to remove this const.
  const std::set<std::string> device_whitelist_;
  std::vector<OnConnectionChangedCallback> connectivity_listeners_;

  // State for tracking where we are in our attempts to connect to a service.
  bool connecting_service_reset_pending_{false};
  bool have_called_connect_{false};
  std::shared_ptr<org::chromium::flimflam::ServiceProxy> connecting_service_;
  base::CancelableClosure on_connect_success_;

  // State for tracking our online connectivity.
  std::map<dbus::ObjectPath, DeviceState> devices_;
  NetworkState connectivity_state_{NetworkState::kOffline};

  base::WeakPtrFactory<ShillClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShillClient);
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_SHILL_CLIENT_H_
