// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_NETWORK_PROXY_
#define SHILL_MODEM_GSM_NETWORK_PROXY_

#include <string>

#include "shill/dbus_bindings/modem-gsm-network.h"
#include "shill/modem_gsm_network_proxy_interface.h"

namespace shill {

class ModemGSMNetworkProxy : public ModemGSMNetworkProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gsm.Network DBus object proxy at |path|
  // owned by |service|. Caught signals will be dispatched to |delegate|.
  ModemGSMNetworkProxy(ModemGSMNetworkProxyDelegate *delegate,
                       DBus::Connection *connection,
                       const std::string &path,
                       const std::string &service);
  virtual ~ModemGSMNetworkProxy();

  // Inherited from ModemGSMNetworkProxyInterface.
  virtual void GetRegistrationInfo(AsyncCallHandler *call_handler, int timeout);
  virtual uint32 GetSignalQuality();
  virtual void Register(const std::string &network_id,
                        AsyncCallHandler *call_handler, int timeout);
  virtual void Scan(AsyncCallHandler *call_handler, int timeout);
  virtual uint32 AccessTechnology();

 private:
  class Proxy
      : public org::freedesktop::ModemManager::Modem::Gsm::Network_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(ModemGSMNetworkProxyDelegate *delegate,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem::Gsm::Network_proxy.
    virtual void SignalQuality(const uint32 &quality);
    virtual void RegistrationInfo(const uint32_t &status,
                                  const std::string &operator_code,
                                  const std::string &operator_name);
    virtual void NetworkMode(const uint32_t &mode);

    // Method callbacks inherited from ModemManager::Modem::Gsm::Network_proxy.
    virtual void RegisterCallback(const DBus::Error &dberror, void *data);
    virtual void GetRegistrationInfoCallback(const GSMRegistrationInfo &info,
                                             const DBus::Error &dberror,
                                             void *data);
    virtual void ScanCallback(const GSMScanResults &results,
                              const DBus::Error &dberror, void *data);

    ModemGSMNetworkProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemGSMNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_NETWORK_PROXY_
