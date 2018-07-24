// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MM1_MODEM_MODEMCDMA_PROXY_H_
#define SHILL_CELLULAR_MM1_MODEM_MODEMCDMA_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager1.Modem.ModemCdma.h"
#include "shill/cellular/mm1_modem_modemcdma_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Modem.ModemCdma.
class ModemModemCdmaProxy : public ModemModemCdmaProxyInterface {
 public:
  // Constructs a org.freedesktop.ModemManager1.Modem.ModemCdma DBus
  // object proxy at |path| owned by |service|.
  ModemModemCdmaProxy(DBus::Connection* connection,
                      const std::string& path,
                      const std::string& service);
  ~ModemModemCdmaProxy() override;

  // Inherited methods from ModemModemCdmaProxyInterface.
  void Activate(const std::string& carrier,
                Error* error,
                const ResultCallback& callback,
                int timeout) override;
  void ActivateManual(
      const DBusPropertiesMap& properties,
      Error* error,
      const ResultCallback& callback,
      int timeout) override;

  void set_activation_state_callback(
      const ActivationStateSignalCallback& callback) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::ModemCdma_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    virtual void set_activation_state_callback(
        const ActivationStateSignalCallback& callback);

   private:
    // Signal callbacks inherited from Proxy
    // handle signals
    void ActivationStateChanged(
        const uint32_t& activation_state,
        const uint32_t& activation_error,
        const DBusPropertiesMap& status_changes) override;

    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem::ModemCdmaProxy
    void ActivateCallback(const ::DBus::Error& dberror, void* data) override;
    void ActivateManualCallback(const ::DBus::Error& dberror,
                                void* data) override;
    ActivationStateSignalCallback activation_state_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemModemCdmaProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_MODEM_MODEMCDMA_PROXY_H_
