// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_CARD_PROXY_
#define SHILL_MODEM_GSM_CARD_PROXY_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem-gsm-card.h"
#include "shill/modem_gsm_card_proxy_interface.h"

namespace shill {

class ModemGSMCardProxy : public ModemGSMCardProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gsm.Card DBus object proxy at |path| owned
  // by |service|. Callbacks will be dispatched to |delegate|.
  ModemGSMCardProxy(ModemGSMCardProxyDelegate *delegate,
                    DBus::Connection *connection,
                    const std::string &path,
                    const std::string &service);
  virtual ~ModemGSMCardProxy();

  // Inherited from ModemGSMCardProxyInterface.
  virtual void GetIMEI(AsyncCallHandler *call_handler, int timeout);
  virtual void GetIMSI(AsyncCallHandler *call_handler, int timeout);
  virtual void GetSPN(AsyncCallHandler *call_handler, int timeout);
  virtual void GetMSISDN(AsyncCallHandler *call_handler, int timeout);
  virtual void EnablePIN(const std::string &pin, bool enabled,
                         AsyncCallHandler *call_handler, int timeout);
  virtual void SendPIN(const std::string &pin,
                       AsyncCallHandler *call_handler, int timeout);
  virtual void SendPUK(const std::string &puk, const std::string &pin,
                       AsyncCallHandler *call_handler, int timeout);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         AsyncCallHandler *call_handler, int timeout);
  virtual uint32 EnabledFacilityLocks();

 private:
  class Proxy
      : public org::freedesktop::ModemManager::Modem::Gsm::Card_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(ModemGSMCardProxyDelegate *delegate,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem::Gsm::Card_proxy.
    // [None]

    // Method callbacks inherited from ModemManager::Modem::Gsm::Card_proxy.
    virtual void GetImeiCallback(const std::string &imei,
                                 const DBus::Error &dberror, void *data);
    virtual void GetImsiCallback(const std::string &imsi,
                                 const DBus::Error &dberror, void *data);
    virtual void GetSpnCallback(const std::string &spn,
                                 const DBus::Error &dberror, void *data);
    virtual void GetMsIsdnCallback(const std::string &misisdn,
                                 const DBus::Error &dberror, void *data);
    virtual void EnablePinCallback(const DBus::Error &dberror, void *data);
    virtual void SendPinCallback(const DBus::Error &dberror, void *data);
    virtual void SendPukCallback(const DBus::Error &dberror, void *data);
    virtual void ChangePinCallback(const DBus::Error &dberror, void *data);

    ModemGSMCardProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemGSMCardProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_CARD_PROXY_
