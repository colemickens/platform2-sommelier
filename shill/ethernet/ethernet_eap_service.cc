// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet/ethernet_eap_service.h"

#include <string>

#include <base/strings/stringprintf.h>

#include "shill/eap_credentials.h"
#include "shill/ethernet/ethernet_eap_provider.h"
#include "shill/manager.h"
#include "shill/technology.h"

using std::string;

namespace shill {

EthernetEapService::EthernetEapService(ControlInterface* control_interface,
                                       EventDispatcher* dispatcher,
                                       Metrics* metrics,
                                       Manager* manager)
    : Service(control_interface, dispatcher, metrics, manager,
              Technology::kEthernetEap) {
  SetEapCredentials(new EapCredentials());
  set_friendly_name("Ethernet EAP Parameters");
}

EthernetEapService::~EthernetEapService() = default;

string EthernetEapService::GetStorageIdentifier() const {
  return base::StringPrintf(
      "%s_all", Technology::NameFromIdentifier(technology()).c_str());
}

string EthernetEapService::GetDeviceRpcId(Error* /*error*/) const {
  return "/";
}

void EthernetEapService::OnEapCredentialsChanged(
    Service::UpdateCredentialsReason reason) {
  if (reason == Service::kReasonPropertyUpdate) {
    // Although the has_ever_connected_ field is not used in the
    // same manner as the other services, we still make this call
    // to maintain consistent behavior by the EAP Credential Change
    // call.
    SetHasEverConnected(false);
  }
  manager()->ethernet_eap_provider()->OnCredentialsChanged();
}

bool EthernetEapService::Unload() {
  Service::Unload();
  manager()->ethernet_eap_provider()->OnCredentialsChanged();
  return false;
}

}  // namespace shill
