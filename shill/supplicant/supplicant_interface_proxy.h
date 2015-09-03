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

#ifndef SHILL_SUPPLICANT_SUPPLICANT_INTERFACE_PROXY_H_
#define SHILL_SUPPLICANT_SUPPLICANT_INTERFACE_PROXY_H_

#include <map>
#include <string>

#include <base/macros.h>

#include "shill/dbus_proxies/supplicant-interface.h"
#include "shill/refptr_types.h"
#include "shill/supplicant/supplicant_interface_proxy_interface.h"

namespace shill {

class SupplicantEventDelegateInterface;

// SupplicantInterfaceProxy. provides access to wpa_supplicant's
// network-interface APIs via D-Bus.  This takes a delegate, which
// is an interface that is used to send notifications of supplicant
// events.  This pointer is not owned by SupplicantInterfaceProxy
// and must outlive the proxy.
class SupplicantInterfaceProxy
    : public SupplicantInterfaceProxyInterface {
 public:
  SupplicantInterfaceProxy(SupplicantEventDelegateInterface* delegate,
                           DBus::Connection* bus,
                           const ::DBus::Path& object_path,
                           const char* dbus_addr);
  ~SupplicantInterfaceProxy() override;

  bool AddNetwork(const KeyValueStore& args, std::string* network) override;
  bool EnableHighBitrates() override;
  bool EAPLogon() override;
  bool EAPLogoff() override;
  bool Disconnect() override;
  bool FlushBSS(const uint32_t& age) override;
  bool NetworkReply(const std::string& network,
                    const std::string& field,
                    const std::string& value) override;
  bool Roam(const std::string& addr) override;
  bool Reassociate() override;
  bool Reattach() override;
  bool RemoveAllNetworks() override;
  bool RemoveNetwork(const std::string& network) override;
  bool Scan(const KeyValueStore& args) override;
  bool SelectNetwork(const std::string& network) override;
  bool SetFastReauth(bool enabled) override;
  bool SetRoamThreshold(uint16_t threshold) override;
  bool SetScanInterval(int seconds) override;
  bool SetDisableHighBitrates(bool disable_high_bitrates) override;
  bool SetSchedScan(bool enable) override;
  bool SetScan(bool enable) override;
  bool TDLSDiscover(const std::string& peer) override;
  bool TDLSSetup(const std::string& peer) override;
  bool TDLSStatus(const std::string& peer, std::string* status) override;
  bool TDLSTeardown(const std::string& peer) override;
  bool SetHT40Enable(const std::string& network, bool enable) override;

 private:
  class Proxy : public fi::w1::wpa_supplicant1::Interface_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(SupplicantEventDelegateInterface* delegate,
          DBus::Connection* bus,
          const ::DBus::Path& object_path,
          const char* dbus_addr);
    ~Proxy() override;

   private:
    // signal handlers called by dbus-c++, via
    // fi::w1::wpa_supplicant1::Interface_proxy interface
    void BlobAdded(const std::string& blobname) override;
    void BlobRemoved(const std::string& blobname) override;
    void BSSAdded(const ::DBus::Path& BSS,
                  const std::map<std::string, ::DBus::Variant>
                  &properties) override;
    void BSSRemoved(const ::DBus::Path& BSS) override;
    void Certification(const std::map<std::string, ::DBus::Variant>
                       &properties) override;
    void EAP(const std::string& status, const std::string& parameter) override;
    void NetworkAdded(
        const ::DBus::Path& network,
        const std::map<std::string, ::DBus::Variant>& properties) override;
    void NetworkRemoved(const ::DBus::Path& network) override;
    void NetworkSelected(const ::DBus::Path& network) override;
    void PropertiesChanged(const std::map<std::string, ::DBus::Variant>
                           &properties) override;
    void ScanDone(const bool& success) override;
    void TDLSDiscoverResponse(const std::string& peer_address) override;

    // This pointer is owned by the object that created |this|.  That object
    // MUST destroy |this| before destroying itself.
    SupplicantEventDelegateInterface* delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantInterfaceProxy);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_SUPPLICANT_INTERFACE_PROXY_H_
