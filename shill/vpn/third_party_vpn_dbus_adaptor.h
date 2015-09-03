//
// Copyright (C) 2014 The Android Open Source Project
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
