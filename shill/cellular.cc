// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/modem_cdma_proxy_interface.h"
#include "shill/modem_proxy_interface.h"
#include "shill/modem_simple_proxy_interface.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
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

Cellular::CDMA::CDMA()
    : registration_state_evdo(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      registration_state_1x(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      activation_state(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED) {}

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link_name,
                   int interface_index,
                   Type type,
                   const string &owner,
                   const string &path)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             interface_index),
      type_(type),
      state_(kStateDisabled),
      dbus_owner_(owner),
      dbus_path_(path),
      service_(new CellularService(control_interface,
                                   dispatcher,
                                   manager,
                                   this)),
      service_registered_(false),
      allow_roaming_(false),
      prl_version_(0),
      scanning_(false),
      scan_interval_(0) {
  store_.RegisterConstString(flimflam::kDBusConnectionProperty, &dbus_owner_);
  store_.RegisterConstString(flimflam::kDBusObjectProperty, &dbus_path_);
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
  store_.RegisterConstUint16(flimflam::kPRLVersionProperty, &prl_version_);

  HelpRegisterDerivedStrIntPair(flimflam::kSIMLockStatusProperty,
                                &Cellular::SimLockStatusToProperty,
                                NULL);
  HelpRegisterDerivedStringmaps(flimflam::kFoundNetworksProperty,
                                &Cellular::EnumerateNetworks,
                                NULL);

  store_.RegisterConstBool(flimflam::kScanningProperty, &scanning_);
  store_.RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);

  VLOG(2) << "Cellular device " << link_name_ << " initialized: "
          << GetTypeString();
}

Cellular::~Cellular() {}

std::string Cellular::GetTypeString() {
  switch (type_) {
    case kTypeGSM: return "CellularTypeGSM";
    case kTypeCDMA: return "CellularTypeCDMA";
    default: return StringPrintf("CellularTypeUnknown-%d", type_);
  }
}

std::string Cellular::GetStateString() {
  switch (state_) {
    case kStateDisabled: return "CellularStateDisabled";
    case kStateEnabled: return "CellularStateEnabled";
    case kStateRegistered: return "CellularStateRegistered";
    case kStateConnected: return "CellularStateConnected";
    default: return StringPrintf("CellularStateUnknown-%d", state_);
  }
}

void Cellular::Start() {
  VLOG(2) << __func__ << ": " << GetStateString();
  InitProxies();
  EnableModem();
  if (type_ == kTypeGSM) {
    RegisterGSMModem();
  }
  GetModemStatus();
  GetModemIdentifiers();
  if (type_ == kTypeGSM) {
    GetGSMProperties();
  }
  GetModemInfo();
  GetModemRegistrationState();
  ReportEnabled();
  // TODO(petkov): Device::Start();
}

void Cellular::Stop() {
  proxy_.reset();
  simple_proxy_.reset();
  cdma_proxy_.reset();
  manager_->DeregisterService(service_);
  service_ = NULL;  // Breaks a reference cycle.
  // TODO(petkov): Device::Stop();
}

void Cellular::InitProxies() {
  proxy_.reset(
      ProxyFactory::factory()->CreateModemProxy(dbus_path_, dbus_owner_));
  simple_proxy_.reset(
      ProxyFactory::factory()->CreateModemSimpleProxy(
          dbus_path_, dbus_owner_));
  switch (type_) {
    case kTypeGSM:
      NOTIMPLEMENTED();
      break;
    case kTypeCDMA:
      cdma_proxy_.reset(
          ProxyFactory::factory()->CreateModemCDMAProxy(
              dbus_path_, dbus_owner_));
      break;
    default: NOTREACHED();
  }
}

void Cellular::EnableModem() {
  CHECK_EQ(kStateDisabled, state_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  proxy_->Enable(true);
  state_ = kStateEnabled;
}

void Cellular::GetModemStatus() {
  CHECK_EQ(kStateEnabled, state_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  DBusPropertiesMap properties = simple_proxy_->GetStatus();
  if (DBusProperties::GetString(properties, "carrier", &carrier_) &&
      type_ == kTypeCDMA) {
    // TODO(petkov): Set Cellular.FirmwareImageName and home_provider based on
    // the carrier.
  }
  DBusProperties::GetString(properties, "meid", &meid_);
  DBusProperties::GetString(properties, "imei", &imei_);
  if (DBusProperties::GetString(properties, "imsi", &imsi_) &&
      type_ == kTypeGSM) {
    // TODO(petkov): Set GSM provider.
  }
  DBusProperties::GetString(properties, "esn", &esn_);
  DBusProperties::GetString(properties, "mdn", &mdn_);
  DBusProperties::GetString(properties, "min", &min_);
  DBusProperties::GetUint16(properties, "prl_version", &prl_version_);
  DBusProperties::GetString(
      properties, "firmware_revision", &firmware_revision_);
  // TODO(petkov): Get payment_url/olp_url/usage_url. For now, get these from
  // ModemManager to match flimflam. In the future, provide a plugin API to get
  // these directly from the modem driver.
  if (type_ == kTypeCDMA) {
    // TODO(petkov): Get activation_state.
  }
}

void Cellular::GetModemIdentifiers() {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::GetGSMProperties() {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::RegisterGSMModem() {
  // TODO(petkov): Invoke ModemManager.Modem.Gsm.Network.Register.
  NOTIMPLEMENTED();
}

void Cellular::GetModemInfo() {
  ModemProxyInterface::Info info = proxy_->GetInfo();
  manufacturer_ = info._1;
  model_id_ = info._2;
  hardware_revision_ = info._3;
  VLOG(2) << "ModemInfo: " << manufacturer_ << ", " << model_id_ << ", "
          << hardware_revision_;
}

void Cellular::GetModemRegistrationState() {
  switch (type_) {
    case kTypeGSM:
      GetGSMRegistrationState();
      break;
    case kTypeCDMA:
      GetCDMARegistrationState();
      break;
    default: NOTREACHED();
  }
}

void Cellular::GetCDMARegistrationState() {
  CHECK_EQ(kTypeCDMA, type_);
  cdma_proxy_->GetRegistrationState(&cdma_.registration_state_1x,
                                    &cdma_.registration_state_evdo);
  VLOG(2) << "CDMA Registration: 1x(" << cdma_.registration_state_1x
          << ") EVDO(" << cdma_.registration_state_evdo << ")";
  // TODO(petkov): handle_reported_connect?
}

void Cellular::GetGSMRegistrationState() {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::ReportEnabled() {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
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
