// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MM1_MODEM_PROXY_H_
#define SHILL_CELLULAR_MM1_MODEM_PROXY_H_

#include <string>
#include <vector>

#include "dbus_proxies/org.freedesktop.ModemManager1.Modem.h"
#include "shill/cellular/mm1_modem_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Modem.
class ModemProxy : public ModemProxyInterface {
 public:
  // Constructs a org.freedesktop.ModemManager1.Modem DBus object
  // proxy at |path| owned by |service|.
  ModemProxy(DBus::Connection* connection,
             const std::string& path,
             const std::string& service);
  ~ModemProxy() override;

  // Inherited methods from ModemProxyInterface.
  void Enable(bool enable,
              Error* error,
              const ResultCallback& callback,
              int timeout) override;
  void CreateBearer(const DBusPropertiesMap& properties,
                    Error* error,
                    const DBusPathCallback& callback,
                    int timeout) override;
  void DeleteBearer(const ::DBus::Path& bearer,
                    Error* error,
                    const ResultCallback& callback,
                    int timeout) override;
  void Reset(Error* error,
             const ResultCallback& callback,
             int timeout) override;
  void FactoryReset(const std::string& code,
                    Error* error,
                    const ResultCallback& callback,
                    int timeout) override;
  void SetCurrentCapabilities(const uint32_t& capabilities,
                              Error* error,
                              const ResultCallback& callback,
                              int timeout) override;
  void SetCurrentModes(const ::DBus::Struct<uint32_t, uint32_t>& modes,
                       Error* error,
                       const ResultCallback& callback,
                       int timeout) override;
  void SetCurrentBands(const std::vector<uint32_t>& bands,
                       Error* error,
                       const ResultCallback& callback,
                       int timeout) override;
  void Command(const std::string& cmd,
               const uint32_t& user_timeout,
               Error* error,
               const StringCallback& callback,
               int timeout) override;
  void SetPowerState(const uint32_t& power_state,
                     Error* error,
                     const ResultCallback& callback,
                     int timeout) override;

  void set_state_changed_callback(
      const ModemStateChangedSignalCallback& callback) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    void set_state_changed_callback(
        const ModemStateChangedSignalCallback& callback);

   private:
    // Signal callbacks inherited from Proxy
    // handle signals
    void StateChanged(const int32_t& old,
                      const int32_t &_new,
                      const uint32_t& reason) override;

    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::ModemProxy
    void EnableCallback(const ::DBus::Error& dberror, void* data) override;
    void CreateBearerCallback(const ::DBus::Path& bearer,
        const ::DBus::Error& dberror, void* data) override;
    void DeleteBearerCallback(
        const ::DBus::Error& dberror, void* data) override;
    void ResetCallback(const ::DBus::Error& dberror, void* data) override;
    void FactoryResetCallback(
        const ::DBus::Error& dberror, void* data) override;
    virtual void SetCurrentCapabilitesCallback(const ::DBus::Error& dberror,
                                               void* data);
    void SetCurrentModesCallback(const ::DBus::Error& dberror,
                                 void* data) override;
    void SetCurrentBandsCallback(const ::DBus::Error& dberror,
                                 void* data) override;
    void CommandCallback(const std::string& response,
                         const ::DBus::Error& dberror,
                         void* data) override;
    void SetPowerStateCallback(const ::DBus::Error& dberror,
                               void* data) override;

    ModemStateChangedSignalCallback state_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  template<typename TraceMsgT, typename CallT, typename CallbackT,
      typename... ArgTypes>
      void BeginCall(
          const TraceMsgT& trace_msg, const CallT& call,
          const CallbackT& callback, Error* error, int timeout,
          ArgTypes... rest);

  DISALLOW_COPY_AND_ASSIGN(ModemProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_MODEM_PROXY_H_
