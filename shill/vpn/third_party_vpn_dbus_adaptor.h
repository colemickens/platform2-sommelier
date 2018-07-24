// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_THIRD_PARTY_VPN_DBUS_ADAPTOR_H_
#define SHILL_VPN_THIRD_PARTY_VPN_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <dbus-c++/error.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.ThirdPartyVpn.h"

namespace shill {

class ThirdPartyVpnDriver;

class ThirdPartyVpnAdaptor
    : public org::chromium::flimflam::ThirdPartyVpn_adaptor,
      public DBusAdaptor,
      public ThirdPartyVpnAdaptorInterface {
 public:
  enum ExternalConnectState {
    kStateConnected = 1,
    kStateFailure,
  };

  ThirdPartyVpnAdaptor(DBus::Connection* conn, ThirdPartyVpnDriver* client);
  ~ThirdPartyVpnAdaptor() override;

  // Implementation of ThirdPartyVpnAdaptorInterface
  void EmitPacketReceived(const std::vector<uint8_t>& packet) override;
  void EmitPlatformMessage(uint32_t message) override;

  // Implementation of org::chromium::flimflam::ThirdPartyVpn_adaptor
  std::string SetParameters(
      const std::map<std::string, std::string>& parameters,
      ::DBus::Error& error) override;  // NOLINT
  void UpdateConnectionState(const uint32_t& connection_state,
                             ::DBus::Error& error) override;  // NOLINT
  void SendPacket(const std::vector<uint8_t>& ip_packet,
                  ::DBus::Error& error) override;  // NOLINT

 private:
  ThirdPartyVpnDriver* client_;
};

}  // namespace shill

#endif  // SHILL_VPN_THIRD_PARTY_VPN_DBUS_ADAPTOR_H_
