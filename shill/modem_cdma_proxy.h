// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_CDMA_PROXY_
#define SHILL_MODEM_CDMA_PROXY_

#include "shill/dbus_bindings/modem-cdma.h"
#include "shill/dbus_properties.h"
#include "shill/modem_cdma_proxy_interface.h"

namespace shill {

class ModemCDMAProxy : public ModemCDMAProxyInterface {
 public:
  // Constructs a ModemManager.Modem.CDMA DBus object proxy at |path| owned by
  // |service|. Caught signals will be dispatched to |delegate|.
  ModemCDMAProxy(ModemCDMAProxyDelegate *delegate,
                 DBus::Connection *connection,
                 const std::string &path,
                 const std::string &service);
  virtual ~ModemCDMAProxy();

  // Inherited from ModemCDMAProxyInterface.
  virtual uint32 Activate(const std::string &carrier);
  virtual void GetRegistrationState(uint32 *cdma_1x_state, uint32 *evdo_state);
  virtual uint32 GetSignalQuality();
  virtual const std::string MEID();

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem::Cdma_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(ModemCDMAProxyDelegate *delegate,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem::Cdma_proxy.
    virtual void ActivationStateChanged(
        const uint32 &activation_state,
        const uint32 &activation_error,
        const DBusPropertiesMap &status_changes);
    virtual void SignalQuality(const uint32 &quality);
    virtual void RegistrationStateChanged(const uint32 &cdma_1x_state,
                                          const uint32 &evdo_state);

    ModemCDMAProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemCDMAProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_CDMA_PROXY_
