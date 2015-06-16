// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MM1_MODEM_TIME_PROXY_H_
#define SHILL_CELLULAR_MM1_MODEM_TIME_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager1.Modem.Time.h"
#include "shill/cellular/mm1_modem_time_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Modem.Time.
class ModemTimeProxy : public ModemTimeProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Modem.Time DBus object
  // proxy at |path| owned by |service|.
  ModemTimeProxy(DBus::Connection* connection,
                 const std::string& path,
                 const std::string& service);
  ~ModemTimeProxy() override;

  // Inherited methods from ModemTimeProxyInterface.
  void GetNetworkTime(Error* error,
                      const StringCallback& callback,
                      int timeout) override;

  void set_network_time_changed_callback(
      const NetworkTimeChangedSignalCallback& callback) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::Time_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    void set_network_time_changed_callback(
        const NetworkTimeChangedSignalCallback& callback);

   private:
    // Signal callbacks inherited from Proxy
    // handle signals.
    void NetworkTimeChanged(const std::string& time) override;

    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem::Time_proxy.
    void GetNetworkTimeCallback(const std::string& time,
                                const ::DBus::Error& dberror,
                                void* data) override;

    NetworkTimeChangedSignalCallback network_time_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemTimeProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_MODEM_TIME_PROXY_H_
