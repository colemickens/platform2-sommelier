// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_WIMAX_DEVICE_PROXY_H_
#define SHILL_DBUS_CHROMEOS_WIMAX_DEVICE_PROXY_H_

#include <string>
#include <vector>

#include <base/callback.h>

#include "shill/wimax/wimax_device_proxy_interface.h"
#include "wimax-manager/dbus-proxies.h"

namespace shill {

class ChromeosWiMaxDeviceProxy : public WiMaxDeviceProxyInterface {
 public:
  // Constructs a WiMaxManager.Device DBus object proxy at |rpc_identifier|.
  ChromeosWiMaxDeviceProxy(const scoped_refptr<dbus::Bus>& bus,
                           const std::string& rpc_identifier);
  ~ChromeosWiMaxDeviceProxy() override;

  // Inherited from WiMaxDeviceProxyInterface.
  void Enable(Error* error,
              const ResultCallback& callback,
              int timeout) override;
  void Disable(Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void ScanNetworks(Error* error,
                    const ResultCallback& callback,
                    int timeout) override;
  void Connect(const RpcIdentifier& network,
               const KeyValueStore& parameters,
               Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void Disconnect(Error* error,
                  const ResultCallback& callback,
                  int timeout) override;
  void set_networks_changed_callback(
      const NetworksChangedCallback& callback) override;
  void set_status_changed_callback(
      const StatusChangedCallback& callback) override;
  uint8_t Index(Error* error) override;
  std::string Name(Error* error) override;
  RpcIdentifiers Networks(Error* error) override;

 private:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const std::string& interface_name,
                const PropertyChangedCallback& callback);
    chromeos::dbus_utils::Property<uint8_t> index;
    chromeos::dbus_utils::Property<std::string> name;
    chromeos::dbus_utils::Property<std::vector<dbus::ObjectPath>> networks;

   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  static const char kPropertyIndex[];
  static const char kPropertyName[];
  static const char kPropertyNetworks[];

  // Signal handlers.
  void NetworksChanged(const std::vector<dbus::ObjectPath>& networks);
  void StatusChanged(int32_t status);

  // Status callbacks for async method calls.
  void OnSuccess(const ResultCallback& callback, const std::string& method);
  void OnFailure(const ResultCallback& callback,
                 const std::string& method,
                 chromeos::Error* error);

  // Callback invoked when the value of property |property_name| is changed.
  void OnPropertyChanged(const std::string& property_name);

  // Called when signal is connected to the ObjectProxy.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  std::unique_ptr<org::chromium::WiMaxManager::DeviceProxy> proxy_;
  std::unique_ptr<PropertySet> properties_;
  NetworksChangedCallback networks_changed_callback_;
  StatusChangedCallback status_changed_callback_;

  base::WeakPtrFactory<ChromeosWiMaxDeviceProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ChromeosWiMaxDeviceProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_WIMAX_DEVICE_PROXY_H_
