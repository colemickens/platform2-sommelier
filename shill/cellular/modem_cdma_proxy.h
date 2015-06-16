// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_CDMA_PROXY_H_
#define SHILL_CELLULAR_MODEM_CDMA_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager.Modem.Cdma.h"
#include "shill/cellular/modem_cdma_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.CDMA.
class ModemCDMAProxy : public ModemCDMAProxyInterface {
 public:
  // Constructs a ModemManager.Modem.CDMA DBus object proxy at |path| owned by
  // |service|.
  ModemCDMAProxy(DBus::Connection* connection,
                 const std::string& path,
                 const std::string& service);
  ~ModemCDMAProxy() override;

  // Inherited from ModemCDMAProxyInterface.
  void Activate(const std::string& carrier, Error* error,
                const ActivationResultCallback& callback, int timeout) override;
  void GetRegistrationState(Error* error,
                            const RegistrationStateCallback& callback,
                            int timeout) override;
  void GetSignalQuality(Error* error,
                        const SignalQualityCallback& callback,
                        int timeout) override;
  const std::string MEID() override;

  void set_activation_state_callback(
      const ActivationStateSignalCallback& callback) override;
  void set_signal_quality_callback(
      const SignalQualitySignalCallback& callback) override;
  void set_registration_state_callback(
      const RegistrationStateSignalCallback& callback) override;

 public:
  class Proxy
      : public org::freedesktop::ModemManager::Modem::Cdma_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    void set_activation_state_callback(
        const ActivationStateSignalCallback& callback);
    void set_signal_quality_callback(
        const SignalQualitySignalCallback& callback);
    void set_registration_state_callback(
        const RegistrationStateSignalCallback& callback);

   private:
    // Signal callbacks inherited from ModemManager::Modem::Cdma_proxy.
    void ActivationStateChanged(
        const uint32_t& activation_state,
        const uint32_t& activation_error,
        const DBusPropertiesMap& status_changes) override;
    void SignalQuality(const uint32_t& quality) override;
    void RegistrationStateChanged(const uint32_t& cdma_1x_state,
                                  const uint32_t& evdo_state) override;

    // Method callbacks inherited from ModemManager::Modem::Cdma_proxy.
    void ActivateCallback(const uint32_t& status,
                          const DBus::Error& dberror, void* data) override;
    void GetRegistrationStateCallback(const uint32_t& state_1x,
                                      const uint32_t& state_evdo,
                                      const DBus::Error& error,
                                      void* data) override;
    void GetSignalQualityCallback(const uint32_t& quality,
                                  const DBus::Error& dberror,
                                  void* data) override;

    ActivationStateSignalCallback activation_state_callback_;
    SignalQualitySignalCallback signal_quality_callback_;
    RegistrationStateSignalCallback registration_state_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModemCDMAProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_CDMA_PROXY_H_
