// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_MODEMCDMA_PROXY_
#define SHILL_MM1_MODEM_MODEMCDMA_PROXY_

#include <string>

#include "shill/dbus_bindings/mm1-modem-modemcdma.h"
#include "shill/dbus_properties.h"
#include "shill/mm1_modem_modemcdma_proxy_interface.h"

namespace shill {
namespace mm1 {

class ModemModemCdmaProxy : public ModemModemCdmaProxyInterface {
 public:
  // Constructs a org.freedesktop.ModemManager1.Modem.ModemCdma DBus
  // object proxy at |path| owned by |service|.
  ModemModemCdmaProxy(DBus::Connection *connection,
                      const std::string &path,
                      const std::string &service);
  virtual ~ModemModemCdmaProxy();

  // Inherited methods from ModemModemCdmaProxyInterface.
  virtual void Activate(const std::string &carrier,
                        Error *error,
                        const ResultCallback &callback,
                        int timeout);
  virtual void ActivateManual(
      const DBusPropertiesMap &properties,
      Error *error,
      const ResultCallback &callback,
      int timeout);

  virtual void set_activation_state_callback(
      const ActivationStateSignalCallback &callback);

  // Inherited properties from ModemCdmaProxyInterface.
  virtual std::string Meid();
  virtual std::string Esn();
  virtual uint32_t Sid();
  virtual uint32_t Nid();
  virtual uint32_t Cdma1xRegistrationState();
  virtual uint32_t EvdoRegistrationState();

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::ModemCdma_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

    virtual void set_activation_state_callback(
        const ActivationStateSignalCallback &callback);

   private:
    // Signal callbacks inherited from Proxy
    // handle signals
    void ActivationStateChanged(
        const uint32_t &activation_state,
        const uint32_t &activation_error,
        const DBusPropertiesMap &status_changes);

    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem::ModemCdmaProxy
    virtual void ActivateCallback(const ::DBus::Error& dberror, void *data);
    virtual void ActivateManualCallback(const ::DBus::Error& dberror,
                                        void *data);
    ActivationStateSignalCallback activation_state_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemModemCdmaProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_MODEMCDMA_PROXY_
