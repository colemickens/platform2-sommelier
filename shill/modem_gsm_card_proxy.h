// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_CARD_PROXY_
#define SHILL_MODEM_GSM_CARD_PROXY_

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem-gsm-card.h"
#include "shill/modem_gsm_card_proxy_interface.h"

namespace shill {

class ModemGSMCardProxy : public ModemGSMCardProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gsm.Card DBus object proxy at |path| owned
  // by |service|. Callbacks will be dispatched to |listener|.
  ModemGSMCardProxy(ModemGSMCardProxyListener *listener,
                    DBus::Connection *connection,
                    const std::string &path,
                    const std::string &service);
  virtual ~ModemGSMCardProxy();

  // Inherited from ModemGSMCardProxyInterface.
  virtual std::string GetIMEI();
  virtual std::string GetIMSI();
  virtual std::string GetSPN();
  virtual std::string GetMSISDN();
  virtual void EnablePIN(const std::string &pin, bool enabled);
  virtual void SendPIN(const std::string &pin);
  virtual void SendPUK(const std::string &puk, const std::string &pin);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin);

 private:
  class Proxy
      : public org::freedesktop::ModemManager::Modem::Gsm::Card_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(ModemGSMCardProxyListener *listener,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem::Gsm::Card_proxy.
    // None.

    ModemGSMCardProxyListener *listener_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemGSMCardProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_CARD_PROXY_
