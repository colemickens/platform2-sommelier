// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <netinet/in.h>
#include <linux/if.h>  // Needs definitions from netinet/in.h

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mobile_provider.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular_capability_cdma.h"
#include "shill/cellular_capability_gsm.h"
#include "shill/cellular_capability_universal.h"
#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/scope_logger.h"
#include "shill/store_interface.h"
#include "shill/technology.h"

using base::Bind;
using std::string;
using std::vector;

namespace shill {

// static
const char Cellular::kAllowRoaming[] = "AllowRoaming";

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
                   Metrics *metrics,
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
             metrics,
             manager,
             link_name,
             address,
             interface_index,
             Technology::kCellular),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_path_(path),
      provider_db_(provider_db),
      allow_roaming_(false) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kDBusConnectionProperty, &dbus_owner_);
  store->RegisterConstString(flimflam::kDBusObjectProperty, &dbus_path_);
  HelpRegisterDerivedString(flimflam::kTechnologyFamilyProperty,
                            &Cellular::GetTechnologyFamily,
                            NULL);
  HelpRegisterDerivedBool(flimflam::kCellularAllowRoamingProperty,
                          &Cellular::GetAllowRoaming,
                          &Cellular::SetAllowRoaming);
  store->RegisterConstStringmap(flimflam::kHomeProviderProperty,
                                &home_provider_.ToDict());
  // For now, only a single capability is supported.
  InitCapability(type, ProxyFactory::GetInstance());

  SLOG(Cellular, 2) << "Cellular device " << this->link_name()
                    << " initialized.";
}

Cellular::~Cellular() {
}

bool Cellular::Load(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  if (!storage->ContainsGroup(id)) {
    LOG(WARNING) << "Device is not available in the persistent store: " << id;
    return false;
  }
  storage->GetBool(id, kAllowRoaming, &allow_roaming_);
  return Device::Load(storage);
}

bool Cellular::Save(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  storage->SetBool(id, kAllowRoaming, allow_roaming_);
  return Device::Save(storage);
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

string Cellular::GetTechnologyFamily(Error *error) {
  return capability_->GetTypeString();
}

void Cellular::SetState(State state) {
  SLOG(Cellular, 2) << GetStateString(state_) << " -> "
                    << GetStateString(state);
  state_ = state;
}

void Cellular::HelpRegisterDerivedBool(
    const string &name,
    bool(Cellular::*get)(Error *error),
    void(Cellular::*set)(const bool &value, Error *error)) {
  mutable_store()->RegisterDerivedBool(
      name,
      BoolAccessor(
          new CustomAccessor<Cellular, bool>(this, get, set)));
}

void Cellular::HelpRegisterDerivedString(
    const string &name,
    string(Cellular::*get)(Error *),
    void(Cellular::*set)(const string&, Error *)) {
  mutable_store()->RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Cellular, string>(this, get, set)));
}

void Cellular::Start(Error *error,
                     const EnabledStateChangedCallback &callback) {
  DCHECK(error);
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  if (state_ != kStateDisabled) {
    return;
  }
  capability_->StartModem(error,
                          Bind(&Cellular::OnModemStarted, this, callback));
}

void Cellular::Stop(Error *error,
                    const EnabledStateChangedCallback &callback) {
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  if (service_) {
    // TODO(ers): See whether we can/should do DestroyService() here.
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
  capability_->StopModem(error,
                         Bind(&Cellular::OnModemStopped, this, callback));
}

bool Cellular::IsUnderlyingDeviceEnabled() const {
  return IsEnabledModemState(modem_state_);
}

// static
bool Cellular::IsEnabledModemState(ModemState state) {
  switch (state) {
    case kModemStateUnknown:
    case kModemStateDisabled:
    case kModemStateInitializing:
    case kModemStateLocked:
    case kModemStateDisabling:
    case kModemStateEnabling:
      return false;
    case kModemStateEnabled:
    case kModemStateSearching:
    case kModemStateRegistered:
    case kModemStateDisconnecting:
    case kModemStateConnecting:
    case kModemStateConnected:
      return true;
  }
  return false;
}

void Cellular::OnModemStarted(const EnabledStateChangedCallback &callback,
                              const Error &error) {
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  if (error.IsSuccess() && (state_ == kStateDisabled)) {
    SetState(kStateEnabled);
    // Registration state updates may have been ignored while the
    // modem was not yet marked enabled.
    HandleNewRegistrationState();
  }
  callback.Run(error);
}

void Cellular::OnModemStopped(const EnabledStateChangedCallback &callback,
                              const Error &error) {
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  if (state_ != kStateDisabled)
    SetState(kStateDisabled);
  callback.Run(error);
}

void Cellular::InitCapability(Type type, ProxyFactory *proxy_factory) {
  // TODO(petkov): Consider moving capability construction into a factory that's
  // external to the Cellular class.
  SLOG(Cellular, 2) << __func__ << "(" << type << ")";
  switch (type) {
    case kTypeGSM:
      capability_.reset(new CellularCapabilityGSM(this, proxy_factory));
      break;
    case kTypeCDMA:
      capability_.reset(new CellularCapabilityCDMA(this, proxy_factory));
      break;
    case kTypeUniversal:
      capability_.reset(new CellularCapabilityUniversal(this, proxy_factory));
      break;
    default: NOTREACHED();
  }
}

void Cellular::Activate(const string &carrier,
                        Error *error, const ResultCallback &callback) {
  capability_->Activate(carrier, error, callback);
}

void Cellular::RegisterOnNetwork(const string &network_id,
                                 Error *error,
                                 const ResultCallback &callback) {
  capability_->RegisterOnNetwork(network_id, error, callback);
}

void Cellular::RequirePIN(const string &pin, bool require,
                          Error *error, const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << "(" << require << ")";
  capability_->RequirePIN(pin, require, error, callback);
}

void Cellular::EnterPIN(const string &pin,
                        Error *error, const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  capability_->EnterPIN(pin, error, callback);
}

void Cellular::UnblockPIN(const string &unblock_code,
                          const string &pin,
                          Error *error, const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  capability_->UnblockPIN(unblock_code, pin, error, callback);
}

void Cellular::ChangePIN(const string &old_pin, const string &new_pin,
                         Error *error, const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  capability_->ChangePIN(old_pin, new_pin, error, callback);
}

void Cellular::Scan(Error *error) {
  // TODO(ers): for now report immediate success or failure.
  capability_->Scan(error, ResultCallback());
}

void Cellular::HandleNewRegistrationState() {
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  if (!capability_->IsRegistered()) {
    DestroyService();
    if (state_ == kStateLinked ||
        state_ == kStateConnected ||
        state_ == kStateRegistered) {
      SetState(kStateEnabled);
    }
    return;
  }
  // In Disabled state, defer creating a service until fully
  // enabled. UI will ignore the appearance of a new service
  // on a disabled device.
  if (state_ == kStateDisabled) {
    return;
  }
  if (state_ == kStateEnabled) {
    SetState(kStateRegistered);
  }
  if (!service_.get()) {
    CreateService();
  }
  capability_->GetSignalQuality();
  if (state_ == kStateRegistered && modem_state_ == kModemStateConnected)
    OnConnected();
  service_->SetNetworkTechnology(capability_->GetNetworkTechnologyString());
  service_->SetRoamingState(capability_->GetRoamingStateString());
}

void Cellular::HandleNewSignalQuality(uint32 strength) {
  SLOG(Cellular, 2) << "Signal strength: " << strength;
  if (service_) {
    service_->SetStrength(strength);
  }
}

void Cellular::CreateService() {
  SLOG(Cellular, 2) << __func__;
  CHECK(!service_.get());
  service_ =
      new CellularService(control_interface(), dispatcher(), metrics(),
                          manager(), this);
  capability_->OnServiceCreated();
  manager()->RegisterService(service_);
}

void Cellular::DestroyService() {
  SLOG(Cellular, 2) << __func__;
  DestroyIPConfig();
  if (service_) {
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
  SelectService(NULL);
}

bool Cellular::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kCellular;
}

void Cellular::Connect(Error *error) {
  SLOG(Cellular, 2) << __func__;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    Error::PopulateAndLog(error, Error::kAlreadyConnected,
                          "Already connected; connection request ignored.");
    return;
  }
  CHECK_EQ(kStateRegistered, state_);

  if (!capability_->AllowRoaming() &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Error::PopulateAndLog(error, Error::kNotOnHomeNetwork,
                          "Roaming disallowed; connection request ignored.");
    return;
  }

  DBusPropertiesMap properties;
  capability_->SetupConnectProperties(&properties);
  // TODO(njw): Should a weak pointer be used here instead?
  // Would require something like a WeakPtrFactory on the class.
  ResultCallback cb = Bind(&Cellular::OnConnectReply, this);
  OnConnecting();
  capability_->Connect(properties, error, cb);
}

// Note that there's no ResultCallback argument to this,
// since Connect() isn't yet passed one.
void Cellular::OnConnectReply(const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    OnConnected();
  else
    OnConnectFailed(error);
}

void Cellular::OnConnecting() {
  if (service_)
    service_->SetState(Service::kStateAssociating);
}

void Cellular::OnConnected() {
  SLOG(Cellular, 2) << __func__;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    VLOG(2) << "Already connected";
    return;
  }
  SetState(kStateConnected);
  if (!capability_->AllowRoaming() &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Disconnect(NULL);
  } else {
    EstablishLink();
  }
}

void Cellular::OnConnectFailed(const Error &error) {
  service_->SetFailure(Service::kFailureUnknown);
}

void Cellular::Disconnect(Error *error) {
  SLOG(Cellular, 2) << __func__;
  if (state_ != kStateConnected && state_ != kStateLinked) {
    Error::PopulateAndLog(
        error, Error::kNotConnected, "Not connected; request ignored.");
    return;
  }
  // TODO(njw): See Connect() about possible weak ptr for 'this'.
  ResultCallback cb = Bind(&Cellular::OnDisconnectReply, this);
  capability_->Disconnect(error, cb);
}

// Note that there's no ResultCallback argument to this,
// since Disconnect() isn't yet passed one.
void Cellular::OnDisconnectReply(const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    OnDisconnected();
  else
    OnDisconnectFailed();
}

void Cellular::OnDisconnected() {
  SLOG(Cellular, 2) << __func__;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    SetState(kStateRegistered);
    SetServiceFailureSilent(Service::kFailureUnknown);
    DestroyIPConfig();
    rtnl_handler()->SetInterfaceFlags(interface_index(), 0, IFF_UP);
  } else {
    LOG(WARNING) << "Disconnect occurred while in state "
                 << GetStateString(state_);
  }
}

void Cellular::OnDisconnectFailed() {
  // TODO(ers): Signal failure.
}

void Cellular::EstablishLink() {
  SLOG(Cellular, 2) << __func__;
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
    if (AcquireIPConfig()) {
      SelectService(service_);
      SetServiceState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
  } else if ((flags & IFF_UP) == 0 && state_ == kStateLinked) {
    SetState(kStateConnected);
    DestroyService();
  }
}

void Cellular::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  capability_->OnDBusPropertiesChanged(interface,
                                       changed_properties,
                                       invalidated_properties);
}

void Cellular::set_home_provider(const Operator &oper) {
  home_provider_.CopyFrom(oper);
}

string Cellular::CreateFriendlyServiceName() {
  SLOG(Cellular, 2) << __func__;
  return capability_.get() ? capability_->CreateFriendlyServiceName() : "";
}

void Cellular::OnModemStateChanged(ModemState old_state,
                                   ModemState new_state,
                                   uint32 /*reason*/) {
  if (old_state == new_state) {
    return;
  }
  set_modem_state(new_state);
  switch (new_state) {
    case kModemStateDisabled:
      SetEnabled(false);
      break;
    case kModemStateEnabled:
    case kModemStateSearching:
      // Note: we only handle changes to Enabled from the Registered
      // state here. Changes from Disabled to Enabled are handled in
      // the DBusPropertiesChanged handler.
      if (old_state == kModemStateRegistered) {
        capability_->SetUnregistered(new_state == kModemStateSearching);
        HandleNewRegistrationState();
      }
    case kModemStateRegistered:
      if (old_state == kModemStateConnected ||
          old_state == kModemStateConnecting ||
          old_state == kModemStateDisconnecting)
        OnDisconnected();
      break;
    case kModemStateConnecting:
      OnConnecting();
      break;
    case kModemStateConnected:
      OnConnected();
      break;
    default:
      break;
  }
}

void Cellular::SetAllowRoaming(const bool &value, Error */*error*/) {
  SLOG(Cellular, 2) << __func__
                    << "(" << allow_roaming_ << "->" << value << ")";
  if (allow_roaming_ == value) {
    return;
  }
  allow_roaming_ = value;
  manager()->SaveActiveProfile();

  // Use AllowRoaming() instead of allow_roaming_ in order to
  // incorporate provider preferences when evaluating if a disconnect
  // is required.
  if (!capability_->AllowRoaming() &&
      capability_->GetRoamingStateString() == flimflam::kRoamingStateRoaming) {
    Error error;
    Disconnect(&error);
  }
  adaptor()->EmitBoolChanged(flimflam::kCellularAllowRoamingProperty, value);
}

}  // namespace shill
