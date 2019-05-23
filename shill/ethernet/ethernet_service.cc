// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet/ethernet_service.h"

#include <netinet/ether.h>
#include <linux/if.h>  // NOLINT - Needs definitions from netinet/ether.h
#include <stdio.h>
#include <time.h>

#include <string>

#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/eap_credentials.h"
#include "shill/ethernet/ethernet.h"
#include "shill/ethernet/ethernet_provider.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/store_interface.h"

using std::string;

namespace shill {

namespace {

constexpr char kAutoConnNoCarrier[] = "no carrier";

}  // namespace

constexpr char EthernetService::kDefaultEthernetDeviceIdentifier[];

EthernetService::EthernetService(ControlInterface* control_interface,
                                 EventDispatcher* dispatcher,
                                 Metrics* metrics,
                                 Manager* manager,
                                 const Properties& props)
    : EthernetService(control_interface,
                      dispatcher,
                      metrics,
                      manager,
                      Technology::kEthernet,
                      props) {
  SetUp();
}

EthernetService::EthernetService(ControlInterface* control_interface,
                                 EventDispatcher* dispatcher,
                                 Metrics* metrics,
                                 Manager* manager,
                                 Technology::Identifier technology,
                                 const Properties& props)
    : Service(control_interface, dispatcher, metrics, manager, technology),
      props_(props) {}

EthernetService::~EthernetService() { }

void EthernetService::SetUp() {
  SetConnectable(true);
  SetAutoConnect(true);
  set_friendly_name("Ethernet");
  SetStrength(kStrengthMax);

  // Now that |this| is a fully constructed EthernetService, synchronize
  // observers with our current state, and emit the appropriate change
  // notifications. (Initial observer state may have been set in our base
  // class.)
  NotifyIfVisibilityChanged();
}

void EthernetService::Connect(Error* error, const char* reason) {
  Service::Connect(error, reason);
  if (props_.ethernet_) {
    props_.ethernet_->ConnectTo(this);
  }
}

void EthernetService::Disconnect(Error* error, const char* reason) {
  Service::Disconnect(error, reason);
  if (props_.ethernet_) {
    props_.ethernet_->DisconnectFrom(this);
  }
}

string EthernetService::GetDeviceRpcId(Error* error) const {
  if (!props_.ethernet_) {
    error->Populate(Error::kNotFound, "Not associated with a device");
    return control_interface()->NullRPCIdentifier();
  }
  return props_.ethernet_->GetRpcIdentifier();
}

string EthernetService::GetStorageIdentifier() const {
  return props_.ethernet_
             ? base::StringPrintf(
                   "%s_%s",
                   Technology::NameFromIdentifier(technology()).c_str(),
                   props_.ethernet_->address().c_str())
             : props_.storage_id_;
}

bool EthernetService::IsAutoConnectByDefault() const {
  return true;
}

bool EthernetService::SetAutoConnectFull(const bool& connect,
                                         Error* error) {
  if (!connect) {
    Error::PopulateAndLog(
        FROM_HERE, error, Error::kInvalidArguments,
        "Auto-connect on Ethernet services must not be disabled.");
    return false;
  }
  return Service::SetAutoConnectFull(connect, error);
}

void EthernetService::Remove(Error* error) {
  error->Populate(Error::kNotSupported);
}

bool EthernetService::IsVisible() const {
  return props_.ethernet_ && props_.ethernet_->link_up();
}

bool EthernetService::IsAutoConnectable(const char** reason) const {
  if (!Service::IsAutoConnectable(reason)) {
    return false;
  }
  if (!props_.ethernet_ || !props_.ethernet_->link_up()) {
    *reason = kAutoConnNoCarrier;
    return false;
  }
  return true;
}

bool EthernetService::Load(StoreInterface* storage) {
  if (!Service::Load(storage)) {
    return false;
  }

  StoreInterface* store = manager()->ActiveProfile()->GetStorage();
  mutable_static_ip_parameters()->Load(store, kDefaultEthernetDeviceIdentifier);
  return true;
}

bool EthernetService::Save(StoreInterface* storage) {
  if (!Service::Save(storage)) {
    return false;
  }

  if (!IsRemembered()) {
    return true;
  }

  StoreInterface* store = manager()->ActiveProfile()->GetStorage();
  mutable_static_ip_parameters()->Save(store,
                                       kDefaultEthernetDeviceIdentifier);
  store->Flush();
  // Now that the IP parameters are saved to the ethernet_any profile, load it
  // into the in-memory ethernet_any service.
  return manager()->ethernet_provider()->LoadGenericEthernetService();
}

void EthernetService::OnVisibilityChanged() {
  NotifyIfVisibilityChanged();
}

string EthernetService::GetTethering(Error* /*error*/) const {
  return props_.ethernet_ && props_.ethernet_->IsConnectedViaTether()
             ? kTetheringConfirmedState
             : kTetheringNotDetectedState;
}

}  // namespace shill
