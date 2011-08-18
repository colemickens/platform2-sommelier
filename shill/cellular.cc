// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <netinet/in.h>
#include <linux/if.h>

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
#include "shill/modem_simple_proxy_interface.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/shill_event.h"

using std::make_pair;
using std::string;
using std::vector;

namespace shill {

const char Cellular::kConnectPropertyPhoneNumber[] = "number";
const char Cellular::kPhoneNumberCDMA[] = "#777";
const char Cellular::kPhoneNumberGSM[] = "*99#";

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
      activation_state(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED),
      prl_version(0) {}

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link_name,
                   const std::string &address,
                   int interface_index,
                   Type type,
                   const string &owner,
                   const string &path)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             address,
             interface_index),
      type_(type),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_path_(path),
      task_factory_(this),
      allow_roaming_(false),
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
  store_.RegisterConstUint16(flimflam::kPRLVersionProperty, &cdma_.prl_version);

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

string Cellular::GetTypeString() const {
  switch (type_) {
    case kTypeGSM: return "CellularTypeGSM";
    case kTypeCDMA: return "CellularTypeCDMA";
    default: NOTREACHED();
  }
  return StringPrintf("CellularTypeUnknown-%d", type_);
}

// static
string Cellular::GetStateString(State state) {
  switch (state) {
    case kStateDisabled: return "CellularStateDisabled";
    case kStateEnabled: return "CellularStateEnabled";
    case kStateRegistered: return "CellularStateRegistered";
    case kStateConnected: return "CellularStateConnected";
    case kStateLinked: return "CellularStateLinked";
    default: NOTREACHED();
  }
  return StringPrintf("CellularStateUnknown-%d", state);
}

// static
string Cellular::GetCDMAActivationStateString(uint32 state) {
  switch (state) {
    case MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED:
      return flimflam::kActivationStateActivated;
    case MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING:
      return flimflam::kActivationStateActivating;
    case MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED:
      return flimflam::kActivationStateNotActivated;
    case MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED:
      return flimflam::kActivationStatePartiallyActivated;
    default:
      return flimflam::kActivationStateUnknown;
  }
}

// static
string Cellular::GetCDMAActivationErrorString(uint32 error) {
  switch (error) {
    case MM_MODEM_CDMA_ACTIVATION_ERROR_WRONG_RADIO_INTERFACE:
      return flimflam::kErrorNeedEvdo;
    case MM_MODEM_CDMA_ACTIVATION_ERROR_ROAMING:
      return flimflam::kErrorNeedHomeNetwork;
    case MM_MODEM_CDMA_ACTIVATION_ERROR_COULD_NOT_CONNECT:
    case MM_MODEM_CDMA_ACTIVATION_ERROR_SECURITY_AUTHENTICATION_FAILED:
    case MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED:
      return flimflam::kErrorOtaspFailed;
    case MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR:
      return "";
    case MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL:
    default:
      return flimflam::kErrorActivationFailed;
  }
}

void Cellular::SetState(State state) {
  VLOG(2) << GetStateString(state_) << " -> " << GetStateString(state);
  state_ = state;
}

void Cellular::Start() {
  LOG(INFO) << __func__ << ": " << GetStateString(state_);
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
  // TODO(petkov): Device::Start();
}

void Cellular::Stop() {
  proxy_.reset();
  simple_proxy_.reset();
  cdma_proxy_.reset();
  manager_->DeregisterService(service_);
  service_ = NULL;  // Breaks a reference cycle.
  SetState(kStateDisabled);
  RTNLHandler::GetInstance()->SetInterfaceFlags(interface_index_, 0, IFF_UP);
  // TODO(petkov): Device::Stop();
}

void Cellular::InitProxies() {
  VLOG(2) << __func__;
  proxy_.reset(
      ProxyFactory::factory()->CreateModemProxy(this, dbus_path_, dbus_owner_));
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
              this, dbus_path_, dbus_owner_));
      break;
    default: NOTREACHED();
  }
}

void Cellular::EnableModem() {
  CHECK_EQ(kStateDisabled, state_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  proxy_->Enable(true);
  SetState(kStateEnabled);
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
  DBusProperties::GetString(
      properties, "firmware_revision", &firmware_revision_);

  // TODO(petkov): Handle "state".

  if (type_ == kTypeCDMA) {
    DBusProperties::GetUint32(
        properties, "activation_state", &cdma_.activation_state);
    DBusProperties::GetUint16(properties, "prl_version", &cdma_.prl_version);
    // TODO(petkov): For now, get the payment and usage URLs from ModemManager
    // to match flimflam. In the future, provide a plugin API to get these
    // directly from the modem driver.
    DBusProperties::GetString(properties, "payment_url", &cdma_.payment_url);
    DBusProperties::GetString(properties, "usage_url", &cdma_.usage_url);
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
  HandleNewRegistrationState();
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

bool Cellular::IsModemRegistered() {
  return cdma_.registration_state_1x !=
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN ||
      cdma_.registration_state_evdo !=
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  // TODO(petkov): Handle GSM states.
}

void Cellular::HandleNewRegistrationState() {
  dispatcher_->PostTask(
      task_factory_.NewRunnableMethod(
          &Cellular::HandleNewRegistrationStateTask));
}

void Cellular::HandleNewRegistrationStateTask() {
  VLOG(2) << __func__;
  if (!IsModemRegistered()) {
    if (state_ == kStateLinked) {
      manager_->DeregisterService(service_);
    }
    service_ = NULL;
    if (state_ == kStateLinked ||
        state_ == kStateConnected ||
        state_ == kStateRegistered) {
      SetState(kStateEnabled);
    }
    return;
  }
  if (state_ == kStateEnabled) {
    SetState(kStateRegistered);
  }
  if (!service_.get()) {
    // For now, no endpoint is created. Revisit if necessary.
    CreateService();
  }
  GetModemSignalQuality();
  if (state_ == kStateRegistered && modem_state_ == kModemStateConnected) {
    SetState(kStateConnected);
    EstablishLink();
  }
  // TODO(petkov): Update the service.
}

void Cellular::GetModemSignalQuality() {
  VLOG(2) << __func__;
  uint32 strength = 0;
  switch (type_) {
    case kTypeGSM:
      strength = GetGSMSignalQuality();
      break;
    case kTypeCDMA:
      strength = GetCDMASignalQuality();
      break;
    default: NOTREACHED();
  }
  HandleNewSignalQuality(strength);
}

uint32 Cellular::GetCDMASignalQuality() {
  VLOG(2) << __func__;
  CHECK_EQ(kTypeCDMA, type_);
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  return cdma_proxy_->GetSignalQuality();
}

uint32 Cellular::GetGSMSignalQuality() {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
  return 0;
}

void Cellular::HandleNewSignalQuality(uint32 strength) {
  VLOG(2) << "Signal strength: " << strength;
  if (service_.get()) {
    service_->set_strength(strength);
  }
}

void Cellular::CreateService() {
  VLOG(2) << __func__;
  CHECK(!service_.get());
  service_ =
      new CellularService(control_interface_, dispatcher_, manager_, this);
  switch (type_) {
    case kTypeGSM:
      service_->set_activation_state(flimflam::kActivationStateActivated);
      break;
    case kTypeCDMA:
      service_->set_payment_url(cdma_.payment_url);
      service_->set_usage_url(cdma_.usage_url);
      HandleNewCDMAActivationState(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR);
      break;
    default:
      NOTREACHED();
  }
  // TODO(petkov): Set operator.
}

bool Cellular::TechnologyIs(const Device::Technology type) const {
  return type == Device::kCellular;
}

void Cellular::Connect() {
  VLOG(2) << __func__;
  if (state_ == kStateConnected ||
      state_ == kStateLinked) {
    return;
  }
  CHECK_EQ(kStateRegistered, state_);
  DBusPropertiesMap properties;
  const char *phone_number = NULL;
  switch (type_) {
    case kTypeGSM:
      phone_number = kPhoneNumberGSM;
      break;
    case kTypeCDMA:
      phone_number = kPhoneNumberCDMA;
      break;
    default: NOTREACHED();
  }
  properties[kConnectPropertyPhoneNumber].writer().append_string(phone_number);
  // TODO(petkov): Setup apn and "home_only".

  // Defer connect because we may be in a dbus-c++ callback.
  dispatcher_->PostTask(
      task_factory_.NewRunnableMethod(&Cellular::ConnectTask, properties));
}

void Cellular::ConnectTask(const DBusPropertiesMap &properties) {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  simple_proxy_->Connect(properties);
  SetState(kStateConnected);
  EstablishLink();
}

void Cellular::EstablishLink() {
  VLOG(2) << __func__;
  CHECK_EQ(kStateConnected, state_);
  unsigned int flags = 0;
  if (manager_->device_info()->GetFlags(interface_index_, &flags) &&
      (flags & IFF_UP) != 0) {
    LinkEvent(flags, IFF_UP);
    return;
  }
  // TODO(petkov): Provide a timeout for a failed link-up request.
  RTNLHandler::GetInstance()->SetInterfaceFlags(
      interface_index_, IFF_UP, IFF_UP);
}

void Cellular::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_UP) != 0 && state_ == kStateConnected) {
    LOG(INFO) << link_name_ << " is up.";
    SetState(kStateLinked);
    manager_->RegisterService(service_);
    // TODO(petkov): For GSM, remember the APN.
    LOG_IF(ERROR, !AcquireDHCPConfig()) << "Unable to acquire DHCP config.";
  } else if ((flags & IFF_UP) == 0 && state_ == kStateLinked) {
    SetState(kStateConnected);
    manager_->DeregisterService(service_);
    DestroyIPConfig();
  }
}

void Cellular::Activate(const string &carrier) {
  CHECK_EQ(kTypeCDMA, type_);
  // Defer connect because we may be in a dbus-c++ callback.
  dispatcher_->PostTask(
      task_factory_.NewRunnableMethod(&Cellular::ActivateTask, carrier));
}

void Cellular::ActivateTask(const string &carrier) {
  VLOG(2) << __func__ << "(" << carrier << ")";
  if (state_ != kStateEnabled && state_ != kStateRegistered) {
    LOG(ERROR) << "Unable to activate in " << GetStateString(state_);
    return;
  }
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 status = cdma_proxy_->Activate(carrier);
  if (status == MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR) {
    cdma_.activation_state = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  }
  HandleNewCDMAActivationState(status);
}

void Cellular::HandleNewCDMAActivationState(uint32 error) {
  if (!service_.get()) {
    return;
  }
  service_->set_activation_state(
      GetCDMAActivationStateString(cdma_.activation_state));
  service_->set_error(GetCDMAActivationErrorString(error));
}

void Cellular::OnCDMAActivationStateChanged(
    uint32 activation_state,
    uint32 activation_error,
    const DBusPropertiesMap &status_changes) {
  CHECK_EQ(kTypeCDMA, type_);
  DBusProperties::GetString(status_changes, "mdn", &mdn_);
  DBusProperties::GetString(status_changes, "min", &min_);
  if (DBusProperties::GetString(
          status_changes, "payment_url", &cdma_.payment_url) &&
      service_.get()) {
    service_->set_payment_url(cdma_.payment_url);
  }
  cdma_.activation_state = activation_state;
  HandleNewCDMAActivationState(activation_error);
}

void Cellular::OnCDMARegistrationStateChanged(uint32 state_1x,
                                              uint32 state_evdo) {
  CHECK_EQ(kTypeCDMA, type_);
  cdma_.registration_state_1x = state_1x;
  cdma_.registration_state_evdo = state_evdo;
  HandleNewRegistrationState();
}

void Cellular::OnCDMASignalQualityChanged(uint32 strength) {
  CHECK_EQ(kTypeCDMA, type_);
  HandleNewSignalQuality(strength);
}

void Cellular::OnModemStateChanged(uint32 old_state,
                                   uint32 new_state,
                                   uint32 reason) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
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
