//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_CELLULAR_MODEM_GSM_NETWORK_PROXY_H_
#define SHILL_CELLULAR_MODEM_GSM_NETWORK_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager.Modem.Gsm.Network.h"
#include "shill/cellular/modem_gsm_network_proxy_interface.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.Gsm.Network.
class ModemGSMNetworkProxy : public ModemGSMNetworkProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gsm.Network DBus object proxy at |path|
  // owned by |service|.
  ModemGSMNetworkProxy(DBus::Connection* connection,
                       const std::string& path,
                       const std::string& service);
  ~ModemGSMNetworkProxy() override;

  // Inherited from ModemGSMNetworkProxyInterface.
  void GetRegistrationInfo(Error* error,
                           const RegistrationInfoCallback& callback,
                           int timeout) override;
  void GetSignalQuality(Error* error,
                        const SignalQualityCallback& callback,
                        int timeout) override;
  void Register(const std::string& network_id,
                Error* error, const ResultCallback& callback,
                int timeout) override;
  void Scan(Error* error, const ScanResultsCallback& callback,
            int timeout) override;
  uint32_t AccessTechnology() override;

  void set_signal_quality_callback(
      const SignalQualitySignalCallback& callback) override;
  void set_network_mode_callback(
      const NetworkModeSignalCallback& callback) override;
  void set_registration_info_callback(
      const RegistrationInfoSignalCallback& callback) override;

 private:
  class Proxy
      : public org::freedesktop::ModemManager::Modem::Gsm::Network_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    virtual void set_signal_quality_callback(
        const SignalQualitySignalCallback& callback);
    virtual void set_network_mode_callback(
        const NetworkModeSignalCallback& callback);
    virtual void set_registration_info_callback(
        const RegistrationInfoSignalCallback& callback);

   private:
    // Signal callbacks inherited from ModemManager::Modem::Gsm::Network_proxy.
    void SignalQuality(const uint32_t& quality) override;
    void RegistrationInfo(const uint32_t& status,
                          const std::string& operator_code,
                          const std::string& operator_name) override;
    void NetworkMode(const uint32_t& mode) override;

    // Method callbacks inherited from ModemManager::Modem::Gsm::Network_proxy.
    void RegisterCallback(const DBus::Error& dberror, void* data) override;
    void GetRegistrationInfoCallback(const GSMRegistrationInfo& info,
                                     const DBus::Error& dberror,
                                     void* data) override;
    void GetSignalQualityCallback(const uint32_t& quality,
                                  const DBus::Error& dberror,
                                  void* data) override;
    void ScanCallback(const GSMScanResults& results,
                      const DBus::Error& dberror, void* data) override;

    SignalQualitySignalCallback signal_quality_callback_;
    RegistrationInfoSignalCallback registration_info_callback_;
    NetworkModeSignalCallback network_mode_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemGSMNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_GSM_NETWORK_PROXY_H_
