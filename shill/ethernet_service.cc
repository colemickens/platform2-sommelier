// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet_service.h"

#include <time.h>
#include <stdio.h>
#include <netinet/ether.h>
#include <linux/if.h>

#include <string>

#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/eap_credentials.h"
#include "shill/ethernet.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/profile.h"

using std::string;

namespace shill {

// static
const char EthernetService::kServiceType[] = "ethernet";

EthernetService::EthernetService(ControlInterface *control_interface,
                                 EventDispatcher *dispatcher,
                                 Metrics *metrics,
                                 Manager *manager,
                                 const EthernetRefPtr &device)
    : Service(control_interface, dispatcher, metrics, manager,
              Technology::kEthernet),
      ethernet_(device) {
  SetConnectable(true);
  SetAutoConnect(true);
  set_friendly_name("Ethernet");
  SetStrength(kStrengthMax);
}

EthernetService::~EthernetService() { }

void EthernetService::Connect(Error *error, const char *reason) {
  Service::Connect(error, reason);
  ethernet_->ConnectTo(this);
}

void EthernetService::Disconnect(Error */*error*/) {
  ethernet_->DisconnectFrom(this);
}

std::string EthernetService::GetDeviceRpcId(Error */*error*/) const {
  return ethernet_->GetRpcIdentifier();
}

string EthernetService::GetStorageIdentifier() const {
  return base::StringPrintf("%s_%s",
                            kServiceType, ethernet_->address().c_str());
}

bool EthernetService::SetAutoConnectFull(const bool &connect,
                                         Error *error) {
  if (!connect) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments,
        "Auto-connect on Ethernet services must not be disabled.");
    return false;
  }
  return Service::SetAutoConnectFull(connect, error);
}

void EthernetService::Remove(Error *error) {
  error->Populate(Error::kNotSupported);
}

string EthernetService::GetTethering(Error */*error*/) const {
  return ethernet_->IsConnectedViaTether() ? kTetheringConfirmedState :
      kTetheringNotDetectedState;
}

}  // namespace shill
