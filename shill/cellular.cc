// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <netinet/in.h>
#include <linux/if.h>  // Needs definitions from netinet/in.h

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular_capability_cdma.h"
#include "shill/cellular_capability_gsm.h"
#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/modem_simple_proxy_interface.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/technology.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

const char Cellular::kConnectPropertyPhoneNumber[] = "number";
const char Cellular::kPropertyIMSI[] = "imsi";

Cellular::Operator::Operator() {
  SetName("");
  SetCode("");
  SetCountry("");
}

Cellular::Operator::~Operator() {}

void Cellular::Operator::CopyFrom(const Operator &oper) {
  dict_ = oper.dict_;
}

const string &Cellular::Operator::GetName() const {
  return dict_.find(flimflam::kOperatorNameKey)->second;
}

void Cellular::Operator::SetName(const string &name) {
  dict_[flimflam::kOperatorNameKey] = name;
}

const string &Cellular::Operator::GetCode() const {
  return dict_.find(flimflam::kOperatorCodeKey)->second;
}

void Cellular::Operator::SetCode(const string &code) {
  dict_[flimflam::kOperatorCodeKey] = code;
}

const string &Cellular::Operator::GetCountry() const {
  return dict_.find(flimflam::kOperatorCountryKey)->second;
}

void Cellular::Operator::SetCountry(const string &country) {
  dict_[flimflam::kOperatorCountryKey] = country;
}

const Stringmap &Cellular::Operator::ToDict() const {
  return dict_;
}

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager,
                   const string &link_name,
                   const string &address,
                   int interface_index,
                   Type type,
                   const string &owner,
                   const string &path,
                   mobile_provider_db *provider_db)
    : Device(control_interface,
             dispatcher,
             manager,
             link_name,
             address,
             interface_index,
             Technology::kCellular),
      proxy_factory_(ProxyFactory::GetInstance()),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_path_(path),
      provider_db_(provider_db),
      task_factory_(this),
      allow_roaming_(false) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  store->RegisterConstString(flimflam::kDBusConnectionProperty, &dbus_owner_);
  store->RegisterConstString(flimflam::kDBusObjectProperty, &dbus_path_);
  store->RegisterBool(flimflam::kCellularAllowRoamingProperty, &allow_roaming_);
  store->RegisterConstString(flimflam::kEsnProperty, &esn_);
  store->RegisterConstString(flimflam::kFirmwareRevisionProperty,
                             &firmware_revision_);
  store->RegisterConstString(flimflam::kHardwareRevisionProperty,
                             &hardware_revision_);
  store->RegisterConstStringmap(flimflam::kHomeProviderProperty,
                                &home_provider_.ToDict());
  store->RegisterConstString(flimflam::kImeiProperty, &imei_);
  store->RegisterConstString(flimflam::kImsiProperty, &imsi_);
  store->RegisterConstString(flimflam::kManufacturerProperty, &manufacturer_);
  store->RegisterConstString(flimflam::kMdnProperty, &mdn_);
  store->RegisterConstString(flimflam::kMeidProperty, &meid_);
  store->RegisterConstString(flimflam::kMinProperty, &min_);
  store->RegisterConstString(flimflam::kModelIDProperty, &model_id_);

  InitCapability(type);  // For now, only a single capability is supported.

  VLOG(2) << "Cellular device " << this->link_name() << " initialized.";
}

Cellular::~Cellular() {}

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

void Cellular::SetState(State state) {
  VLOG(2) << GetStateString(state_) << " -> " << GetStateString(state);
  state_ = state;
}

void Cellular::Start() {
  LOG(INFO) << __func__ << ": " << GetStateString(state_);
  Device::Start();
  capability_->OnDeviceStarted();
  InitProxies();
  EnableModem();
  capability_->Register();
  GetModemStatus();
  capability_->GetIdentifiers();
  capability_->GetProperties();
  GetModemInfo();
  capability_->GetRegistrationState();
}

void Cellular::Stop() {
  capability_->OnDeviceStopped();
  proxy_.reset();
  simple_proxy_.reset();
  manager()->DeregisterService(service_);
  service_ = NULL;  // Breaks a reference cycle.
  SetState(kStateDisabled);
  Device::Stop();
}

void Cellular::InitCapability(Type type) {
  // TODO(petkov): Consider moving capability construction into a factory that's
  // external to the Cellular class.
  VLOG(2) << __func__ << "(" << type << ")";
  switch (type) {
    case kTypeGSM:
      capability_.reset(new CellularCapabilityGSM(this));
      break;
    case kTypeCDMA:
      capability_.reset(new CellularCapabilityCDMA(this));
      break;
    default: NOTREACHED();
  }
}

void Cellular::InitProxies() {
  VLOG(2) << __func__;
  proxy_.reset(proxy_factory_->CreateModemProxy(this, dbus_path_, dbus_owner_));
  simple_proxy_.reset(
      proxy_factory_->CreateModemSimpleProxy(dbus_path_, dbus_owner_));
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
  DBusProperties::GetString(properties, "carrier", &carrier_);
  DBusProperties::GetString(properties, "meid", &meid_);
  DBusProperties::GetString(properties, "imei", &imei_);
  DBusProperties::GetString(properties, kPropertyIMSI, &imsi_);
  DBusProperties::GetString(properties, "esn", &esn_);
  DBusProperties::GetString(properties, "mdn", &mdn_);
  DBusProperties::GetString(properties, "min", &min_);
  DBusProperties::GetString(
      properties, "firmware_revision", &firmware_revision_);

  uint32 state = 0;
  if (DBusProperties::GetUint32(properties, "state", &state)) {
    modem_state_ = static_cast<ModemState>(state);
  }

  capability_->UpdateStatus(properties);
}

void Cellular::Activate(const std::string &carrier, Error *error) {
  capability_->Activate(carrier, error);
}

void Cellular::RegisterOnNetwork(const string &network_id, Error *error) {
  capability_->RegisterOnNetwork(network_id, error);
}

void Cellular::RequirePIN(const string &pin, bool require, Error *error) {
  capability_->RequirePIN(pin, require, error);
}

void Cellular::EnterPIN(const string &pin, Error *error) {
  capability_->EnterPIN(pin, error);
}

void Cellular::UnblockPIN(
    const string &unblock_code, const string &pin, Error *error) {
  capability_->UnblockPIN(unblock_code, pin, error);
}

void Cellular::ChangePIN(
    const string &old_pin, const string &new_pin, Error *error) {
  capability_->ChangePIN(old_pin, new_pin, error);
}

void Cellular::Scan(Error *error) {
  capability_->Scan(error);
}

void Cellular::GetModemInfo() {
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  ModemProxyInterface::Info info = proxy_->GetInfo();
  manufacturer_ = info._1;
  model_id_ = info._2;
  hardware_revision_ = info._3;
  VLOG(2) << "ModemInfo: " << manufacturer_ << ", " << model_id_ << ", "
          << hardware_revision_;
}

void Cellular::HandleNewRegistrationState() {
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &Cellular::HandleNewRegistrationStateTask));
}

void Cellular::HandleNewRegistrationStateTask() {
  VLOG(2) << __func__;
  if (!capability_->IsRegistered()) {
    if (state_ == kStateLinked) {
      manager()->DeregisterService(service_);
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
  capability_->GetSignalQuality();
  if (state_ == kStateRegistered && modem_state_ == kModemStateConnected) {
    SetState(kStateConnected);
    EstablishLink();
  }
  service_->SetNetworkTechnology(capability_->GetNetworkTechnologyString());
  service_->SetRoamingState(capability_->GetRoamingStateString());
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
      new CellularService(control_interface(), dispatcher(), manager(), this);
  capability_->OnServiceCreated();
}

bool Cellular::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kCellular;
}

void Cellular::Connect(Error *error) {
  VLOG(2) << __func__;
  if (state_ == kStateConnected ||
      state_ == kStateLinked) {
    Error::PopulateAndLog(error, Error::kAlreadyConnected,
                          "Already connected; connection request ignored.");
    return;
  }
  CHECK_EQ(kStateRegistered, state_);

  if (!allow_roaming_ &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Error::PopulateAndLog(error, Error::kNotOnHomeNetwork,
                          "Roaming disallowed; connection request ignored.");
    return;
  }

  DBusPropertiesMap properties;
  capability_->SetupConnectProperties(&properties);

  // Defer connect because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
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
  if (manager()->device_info()->GetFlags(interface_index(), &flags) &&
      (flags & IFF_UP) != 0) {
    LinkEvent(flags, IFF_UP);
    return;
  }
  // TODO(petkov): Provide a timeout for a failed link-up request.
  rtnl_handler()->SetInterfaceFlags(interface_index(), IFF_UP, IFF_UP);
}

void Cellular::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_UP) != 0 && state_ == kStateConnected) {
    LOG(INFO) << link_name() << " is up.";
    SetState(kStateLinked);
    manager()->RegisterService(service_);
    // TODO(petkov): For GSM, remember the APN.
    if (AcquireDHCPConfig()) {
      SelectService(service_);
      SetServiceState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
  } else if ((flags & IFF_UP) == 0 && state_ == kStateLinked) {
    SetState(kStateConnected);
    manager()->DeregisterService(service_);
    SelectService(NULL);
    DestroyIPConfig();
  }
}

void Cellular::OnModemStateChanged(uint32 /*old_state*/,
                                   uint32 /*new_state*/,
                                   uint32 /*reason*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void Cellular::OnModemManagerPropertiesChanged(
    const DBusPropertiesMap &properties) {
  capability_->OnModemManagerPropertiesChanged(properties);
}

void Cellular::set_home_provider(const Operator &oper) {
  home_provider_.CopyFrom(oper);
}

}  // namespace shill
