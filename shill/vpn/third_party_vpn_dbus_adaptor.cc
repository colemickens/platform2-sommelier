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

#include "shill/vpn/third_party_vpn_dbus_adaptor.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/logging.h"
#include "shill/service.h"
#include "shill/vpn/third_party_vpn_driver.h"

namespace shill {

namespace Logging {

static auto kModuleLogScope = ScopeLogger::kVPN;
static std::string ObjectID(const ThirdPartyVpnAdaptor* v) {
  return "(third_party_vpn_adaptor)";
}

}  // namespace Logging

namespace {

// The API converts external connection state to internal one
bool ConvertConnectState(
    ThirdPartyVpnAdaptor::ExternalConnectState external_state,
    Service::ConnectState* internal_state) {
  switch (external_state) {
    case ThirdPartyVpnAdaptor::kStateConnected:
      *internal_state = Service::kStateOnline;
      break;
    case ThirdPartyVpnAdaptor::kStateFailure:
      *internal_state = Service::kStateFailure;
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

ThirdPartyVpnAdaptorInterface::~ThirdPartyVpnAdaptorInterface() {}

ThirdPartyVpnAdaptor::ThirdPartyVpnAdaptor(DBus::Connection* conn,
                                           ThirdPartyVpnDriver* client)
    : DBusAdaptor(conn, kObjectPathBase + client->object_path_suffix()),
      client_(client) {}

ThirdPartyVpnAdaptor::~ThirdPartyVpnAdaptor() {}

void ThirdPartyVpnAdaptor::EmitPacketReceived(
    const std::vector<uint8_t>& packet) {
  SLOG(this, 2) << __func__;
  OnPacketReceived(packet);
}

void ThirdPartyVpnAdaptor::EmitPlatformMessage(uint32_t message) {
  SLOG(this, 2) << __func__ << "(" << message << ")";
  OnPlatformMessage(message);
}

std::string ThirdPartyVpnAdaptor::SetParameters(
    const std::map<std::string, std::string>& parameters,
    ::DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  std::string error_message;
  std::string warning_message;
  // TODO(kaliamoorthi): Return warning message to the user via DBUS API.
  client_->SetParameters(parameters, &error_message, &warning_message);
  if (!error_message.empty()) {
    error.set(kErrorResultInvalidArguments, error_message.c_str());
  }
  return warning_message;
}

void ThirdPartyVpnAdaptor::UpdateConnectionState(
    const uint32_t& connection_state,
    ::DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << "(" << connection_state << ")";
  // Externally supported states are from Service::kStateConnected to
  // Service::kStateOnline.
  Service::ConnectState internal_state;
  std::string error_message;
  if (ConvertConnectState(static_cast<ExternalConnectState>(connection_state),
                          &internal_state)) {
    client_->UpdateConnectionState(internal_state, &error_message);
    if (!error_message.empty()) {
      error.set(kErrorResultInvalidArguments, error_message.c_str());
    }
  } else {
    error.set(kErrorResultNotSupported, "Connection state is not supported");
  }
}

void ThirdPartyVpnAdaptor::SendPacket(const std::vector<uint8_t>& ip_packet,
                                      ::DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  std::string error_message;
  client_->SendPacket(ip_packet, &error_message);
  if (!error_message.empty()) {
    error.set(kErrorResultWrongState, error_message.c_str());
  }
}

}  // namespace shill
