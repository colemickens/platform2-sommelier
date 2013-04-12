// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet_eap_service.h"

#include <string>

#include <base/stringprintf.h>

#include "shill/eap_credentials.h"
#include "shill/ethernet_eap_provider.h"
#include "shill/manager.h"
#include "shill/technology.h"

using std::string;

namespace shill {

EthernetEapService::EthernetEapService(ControlInterface *control_interface,
                                       EventDispatcher *dispatcher,
                                       Metrics *metrics,
                                       Manager *manager)
    : Service(control_interface, dispatcher, metrics, manager,
              Technology::kEthernetEap) {
  SetEapCredentials(new EapCredentials());
  set_friendly_name("Ethernet EAP Parameters");
}

EthernetEapService::~EthernetEapService() {}

string EthernetEapService::GetStorageIdentifier() const {
  return StringPrintf("%s_all",
                      Technology::NameFromIdentifier(technology()).c_str());
}

string EthernetEapService::GetDeviceRpcId(Error */*error*/) {
  return "/";
}

void EthernetEapService::OnEapCredentialsChanged() {
  manager()->ethernet_eap_provider()->OnCredentialsChanged();
}

}  // namespace shill


