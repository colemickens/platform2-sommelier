// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_WIMAX_DEVICE_PROXY_INTERFACE_H_
#define SHILL_WIMAX_WIMAX_DEVICE_PROXY_INTERFACE_H_

#include <string>

#include <chromeos/dbus/service_constants.h>

#include "shill/callbacks.h"

namespace shill {

class Error;
class KeyValueStore;

// These are the methods that a WiMaxManager.Device proxy must support. The
// interface is provided so that it can be mocked in tests.
class WiMaxDeviceProxyInterface {
 public:
  using NetworksChangedCallback = base::Callback<void(const RpcIdentifiers&)>;
  using StatusChangedCallback =
      base::Callback<void(wimax_manager::DeviceStatus)>;

  virtual ~WiMaxDeviceProxyInterface() {}

  virtual void Enable(Error* error,
                      const ResultCallback& callback,
                      int timeout) = 0;
  virtual void Disable(Error* error,
                       const ResultCallback& callback,
                       int timeout) = 0;
  virtual void ScanNetworks(Error* error,
                            const ResultCallback& callback,
                            int timeout) = 0;
  virtual void Connect(const RpcIdentifier& network,
                       const KeyValueStore& parameters,
                       Error* error,
                       const ResultCallback& callback,
                       int timeout) = 0;
  virtual void Disconnect(Error* error,
                          const ResultCallback& callback,
                          int timeout) = 0;

  virtual void set_networks_changed_callback(
      const NetworksChangedCallback& callback) = 0;
  virtual void set_status_changed_callback(
      const StatusChangedCallback& callback) = 0;

  // Properties.
  virtual uint8_t Index(Error* error) = 0;
  virtual std::string Name(Error* error) = 0;
  virtual RpcIdentifiers Networks(Error* error) = 0;
};

}  // namespace shill

#endif  // SHILL_WIMAX_WIMAX_DEVICE_PROXY_INTERFACE_H_
