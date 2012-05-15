// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_DEVICE_PROXY_H_
#define SHILL_WIMAX_DEVICE_PROXY_H_

#include <base/callback.h>

#include "shill/dbus_bindings/wimax_manager-device.h"
#include "shill/wimax_device_proxy_interface.h"

namespace shill {

class WiMaxDeviceProxy : public WiMaxDeviceProxyInterface {
 public:
  // Constructs a WiMaxManager.Device DBus object proxy at |path|.
  WiMaxDeviceProxy(DBus::Connection *connection,
                   const DBus::Path &path);
  virtual ~WiMaxDeviceProxy();

  // Inherited from WiMaxDeviceProxyInterface.
  virtual void Enable(Error *error,
                      const ResultCallback &callback,
                      int timeout);
  virtual void Disable(Error *error,
                       const ResultCallback &callback,
                       int timeout);
  virtual void Connect(Error *error,
                       const ResultCallback &callback,
                       int timeout);
  virtual void Disconnect(Error *error,
                          const ResultCallback &callback,
                          int timeout);
  virtual uint8 Index(Error *error);
  virtual std::string Name(Error *error);

 private:
  class Proxy : public org::chromium::WiMaxManager::Device_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const DBus::Path &path);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from WiMaxManager::Device_proxy.
    // [None]

    // Method callbacks inherited from WiMaxManager::Device_proxy.
    virtual void EnableCallback(const DBus::Error &error, void *data);
    virtual void DisableCallback(const DBus::Error &error, void *data);
    virtual void ConnectCallback(const DBus::Error &error, void *data);
    virtual void DisconnectCallback(const DBus::Error &error, void *data);

    static void HandleCallback(const DBus::Error &error, void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  void Invoke(const base::Callback<void(void *, int)> &method,
              Error *error,
              const ResultCallback &callback,
              int timeout);

  static void FromDBusError(const DBus::Error &dbus_error, Error *error);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxDeviceProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_DEVICE_PROXY_H_
