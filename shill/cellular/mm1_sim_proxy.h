// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MM1_SIM_PROXY_H_
#define SHILL_CELLULAR_MM1_SIM_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager1.Sim.h"
#include "shill/cellular/mm1_sim_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Sim.
class SimProxy : public SimProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Sim DBus object
  // proxy at |path| owned by |service|.
  SimProxy(DBus::Connection* connection,
           const std::string& path,
           const std::string& service);
  ~SimProxy() override;

  // Inherited methods from SimProxyInterface.
  void SendPin(const std::string& pin,
               Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void SendPuk(const std::string& puk,
               const std::string& pin,
               Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void EnablePin(const std::string& pin,
                 const bool enabled,
                 Error* error,
                 const ResultCallback& callback,
                 int timeout) override;
  void ChangePin(const std::string& old_pin,
                 const std::string& new_pin,
                 Error* error,
                 const ResultCallback& callback,
                 int timeout) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Sim_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::SimProxy
    void SendPinCallback(const ::DBus::Error& dberror,
                         void* data) override;
    void SendPukCallback(const ::DBus::Error& dberror,
                         void* data) override;
    void EnablePinCallback(const ::DBus::Error& dberror,
                           void* data) override;
    void ChangePinCallback(const ::DBus::Error& dberror,
                           void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  template<typename TraceMsgT, typename CallT, typename CallbackT,
      typename... ArgTypes>
      void BeginCall(
          const TraceMsgT& trace_msg, const CallT& call,
          const CallbackT& callback, Error* error, int timeout,
          ArgTypes... rest);

  DISALLOW_COPY_AND_ASSIGN(SimProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_SIM_PROXY_H_
