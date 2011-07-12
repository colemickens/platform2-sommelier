// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/shill_event.h"

using std::make_pair;
using std::string;
using std::vector;

namespace shill {

Cellular::Network::Network() {
  dict_[flimflam::kStatusProperty] = "";
  dict_[flimflam::kNetworkIdProperty] = "";
  dict_[flimflam::kShortNameProperty] = "";
  dict_[flimflam::kLongNameProperty] = "";
  dict_[flimflam::kTechnologyProperty] = "";
}

Cellular::Network::~Network() {}

const std::string &Cellular::Network::GetStatus() const {
  return dict_.find(flimflam::kStatusProperty)->second;
}

void Cellular::Network::SetStatus(const std::string &status) {
  dict_[flimflam::kStatusProperty] = status;
}

const std::string &Cellular::Network::GetId() const {
  return dict_.find(flimflam::kNetworkIdProperty)->second;
}

void Cellular::Network::SetId(const std::string &id) {
  dict_[flimflam::kNetworkIdProperty] = id;
}

const std::string &Cellular::Network::GetShortName() const {
  return dict_.find(flimflam::kShortNameProperty)->second;
}

void Cellular::Network::SetShortName(const std::string &name) {
  dict_[flimflam::kShortNameProperty] = name;
}

const std::string &Cellular::Network::GetLongName() const {
  return dict_.find(flimflam::kLongNameProperty)->second;
}

void Cellular::Network::SetLongName(const std::string &name) {
  dict_[flimflam::kLongNameProperty] = name;
}

const std::string &Cellular::Network::GetTechnology() const {
  return dict_.find(flimflam::kTechnologyProperty)->second;
}

void Cellular::Network::SetTechnology(const std::string &technology) {
  dict_[flimflam::kTechnologyProperty] = technology;
}

const Stringmap &Cellular::Network::ToDict() const {
  return dict_;
}

Cellular::SimLockStatus::SimLockStatus() {}

Cellular::SimLockStatus::~SimLockStatus() {}

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link,
                   int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link,
             interface_index),
      service_(new CellularService(control_interface,
                                   dispatcher,
                                   this,
                                   manager->ActiveProfile(),
                                   "service-" + link_name())),
      service_registered_(false) {
  store_.RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  store_.RegisterBool(flimflam::kCellularAllowRoamingProperty, &allow_roaming_);
  store_.RegisterConstString(flimflam::kEsnProperty, &esn_);
  store_.RegisterConstString(flimflam::kFirmwareRevisionProperty,
                             &firmware_revision_);
  store_.RegisterConstString(flimflam::kHardwareRevisionProperty,
                             &hardware_revision_);
  store_.RegisterConstString(flimflam::kImeiProperty, &imei_);
  store_.RegisterConstString(flimflam::kImsiProperty, &imsi_);
  store_.RegisterConstString(flimflam::kManufacturerProperty, &manufacturer_);
  store_.RegisterConstString(flimflam::kMdnProperty, &mdn_);
  store_.RegisterConstString(flimflam::kMeidProperty, &meid_);
  store_.RegisterConstString(flimflam::kMinProperty, &min_);
  store_.RegisterConstString(flimflam::kModelIDProperty, &model_id_);
  store_.RegisterConstInt16(flimflam::kPRLVersionProperty, &prl_version_);

  HelpRegisterDerivedStrIntPair(flimflam::kSIMLockStatusProperty,
                                &Cellular::SimLockStatusToProperty,
                                NULL);
  HelpRegisterDerivedStringmaps(flimflam::kFoundNetworksProperty,
                                &Cellular::EnumerateNetworks,
                                NULL);

  store_.RegisterConstBool(flimflam::kScanningProperty, &scanning_);
  store_.RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);

  VLOG(2) << "Cellular device " << link_name() << " initialized.";
}

Cellular::~Cellular() {
  Stop();
}

void Cellular::Start() {
  Device::Start();
}

void Cellular::Stop() {
  manager_->DeregisterService(service_);
  Device::Stop();
}

bool Cellular::TechnologyIs(const Device::Technology type) {
  return type == Device::kCellular;
}

Stringmaps Cellular::EnumerateNetworks() {
  Stringmaps to_return;
  for (vector<Network>::const_iterator it = found_networks_.begin();
       it != found_networks_.end();
       ++it) {
    to_return.push_back(it->ToDict());
  }
  return to_return;
}

StrIntPair Cellular::SimLockStatusToProperty() {
  return StrIntPair(make_pair(flimflam::kSIMLockTypeProperty,
                              sim_lock_status_.lock_type),
                    make_pair(flimflam::kSIMLockRetriesLeftProperty,
                              sim_lock_status_.retries_left));
}

void Cellular::HelpRegisterDerivedStringmaps(
    const string &name,
    Stringmaps(Cellular::*get)(void),
    bool(Cellular::*set)(const Stringmaps&)) {
  store_.RegisterDerivedStringmaps(
      name,
      StringmapsAccessor(
          new CustomAccessor<Cellular, Stringmaps>(this, get, set)));
}

void Cellular::HelpRegisterDerivedStrIntPair(
    const string &name,
    StrIntPair(Cellular::*get)(void),
    bool(Cellular::*set)(const StrIntPair&)) {
  store_.RegisterDerivedStrIntPair(
      name,
      StrIntPairAccessor(
          new CustomAccessor<Cellular, StrIntPair>(this, get, set)));
}

}  // namespace shill
