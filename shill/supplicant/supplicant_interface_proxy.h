// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  SupplicantInterfaceProxy(SupplicantEventDelegateInterface *delegate,
                           DBus::Connection *bus,
                           const ::DBus::Path &object_path,
                           const char *dbus_addr);
  ~SupplicantInterfaceProxy() override;

  ::DBus::Path AddNetwork(
      const std::map<std::string, ::DBus::Variant> &args) override;
  void EnableHighBitrates() override;
  void EAPLogon() override;
  void EAPLogoff() override;
  void Disconnect() override;
  void FlushBSS(const uint32_t &age) override;
  void NetworkReply(const ::DBus::Path &network,
                    const std::string &field,
                    const std::string &value) override;
  void Reassociate() override;
  void Reattach() override;
  void RemoveAllNetworks() override;
  void RemoveNetwork(const ::DBus::Path &network) override;
  void Scan(
      const std::map<std::string, ::DBus::Variant> &args) override;
  void SelectNetwork(const ::DBus::Path &network) override;
  void SetFastReauth(bool enabled) override;
  void SetRoamThreshold(uint16_t threshold) override;
  void SetScanInterval(int seconds) override;
  void SetDisableHighBitrates(bool disable_high_bitrates) override;
  void SetSchedScan(bool enable) override;
  void TDLSDiscover(const std::string &peer) override;
  void TDLSSetup(const std::string &peer) override;
  std::string TDLSStatus(const std::string &peer) override;
  void TDLSTeardown(const std::string &peer) override;
  void SetHT40Enable(const ::DBus::Path &network, bool enable) override;

 private:
  class Proxy : public fi::w1::wpa_supplicant1::Interface_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(SupplicantEventDelegateInterface *delegate,
          DBus::Connection *bus,
          const ::DBus::Path &object_path,
          const char *dbus_addr);
    ~Proxy() override;

   private:
    // signal handlers called by dbus-c++, via
    // fi::w1::wpa_supplicant1::Interface_proxy interface
    void BlobAdded(const std::string &blobname) override;
    void BlobRemoved(const std::string &blobname) override;
    void BSSAdded(const ::DBus::Path &BSS,
                  const std::map<std::string, ::DBus::Variant>
                  &properties) override;
    void BSSRemoved(const ::DBus::Path &BSS) override;
    void Certification(const std::map<std::string, ::DBus::Variant>
                       &properties) override;
    void EAP(const std::string &status, const std::string &parameter) override;
    void NetworkAdded(
        const ::DBus::Path &network,
        const std::map<std::string, ::DBus::Variant> &properties) override;
    void NetworkRemoved(const ::DBus::Path &network) override;
    void NetworkSelected(const ::DBus::Path &network) override;
    void PropertiesChanged(const std::map<std::string, ::DBus::Variant>
                           &properties) override;
    void ScanDone(const bool &success) override;

    // This pointer is owned by the object that created |this|.  That object
    // MUST destroy |this| before destroying itself.
    SupplicantEventDelegateInterface *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantInterfaceProxy);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_SUPPLICANT_INTERFACE_PROXY_H_
