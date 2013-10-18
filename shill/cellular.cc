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
#include <base/callback.h>
#include <base/file_path.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mobile_provider.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular_capability_cdma.h"
#include "shill/cellular_capability_gsm.h"
#include "shill/cellular_capability_universal.h"
#include "shill/cellular_capability_universal_cdma.h"
#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/external_task.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/ppp_device.h"
#include "shill/ppp_device_factory.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/store_interface.h"
#include "shill/technology.h"

using base::Bind;
using base::Closure;
using base::FilePath;
using std::map;
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
  return dict_.find(kOperatorNameKey)->second;
}

void Cellular::Operator::SetName(const string &name) {
  dict_[kOperatorNameKey] = name;
}

const string &Cellular::Operator::GetCode() const {
  return dict_.find(kOperatorCodeKey)->second;
}

void Cellular::Operator::SetCode(const string &code) {
  dict_[kOperatorCodeKey] = code;
}

const string &Cellular::Operator::GetCountry() const {
  return dict_.find(kOperatorCountryKey)->second;
}

void Cellular::Operator::SetCountry(const string &country) {
  dict_[kOperatorCountryKey] = country;
}

const Stringmap &Cellular::Operator::ToDict() const {
  return dict_;
}

Cellular::Cellular(ModemInfo *modem_info,
                   const string &link_name,
                   const string &address,
                   int interface_index,
                   Type type,
                   const string &owner,
                   const string &service,
                   const string &path,
                   ProxyFactory *proxy_factory)
    : Device(modem_info->control_interface(),
             modem_info->dispatcher(),
             modem_info->metrics(),
             modem_info->manager(),
             link_name,
             address,
             interface_index,
             Technology::kCellular),
      weak_ptr_factory_(this),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_service_(service),
      dbus_path_(path),
      modem_info_(modem_info),
      proxy_factory_(proxy_factory),
      ppp_device_factory_(PPPDeviceFactory::GetInstance()),
      allow_roaming_(false),
      explicit_disconnect_(false),
      is_ppp_authenticating_(false) {
  PropertyStore *store = this->mutable_store();
  // TODO(jglasgow): kDBusConnectionProperty is deprecated.
  store->RegisterConstString(kDBusConnectionProperty, &dbus_owner_);
  store->RegisterConstString(kDBusServiceProperty, &dbus_service_);
  store->RegisterConstString(kDBusObjectProperty, &dbus_path_);
  HelpRegisterConstDerivedString(kTechnologyFamilyProperty,
                                 &Cellular::GetTechnologyFamily);
  HelpRegisterDerivedBool(kCellularAllowRoamingProperty,
                          &Cellular::GetAllowRoaming,
                          &Cellular::SetAllowRoaming);
  store->RegisterConstStringmap(kHomeProviderProperty,
                                &home_provider_.ToDict());

  // For now, only a single capability is supported.
  InitCapability(type);

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
    case kStateDisabled:
      return "CellularStateDisabled";
    case kStateEnabled:
      return "CellularStateEnabled";
    case kStateRegistered:
      return "CellularStateRegistered";
    case kStateConnected:
      return "CellularStateConnected";
    case kStateLinked:
      return "CellularStateLinked";
    default:
      NOTREACHED();
  }
  return StringPrintf("CellularStateUnknown-%d", state);
}

// static
string Cellular::GetModemStateString(ModemState modem_state) {
  switch (modem_state) {
    case kModemStateFailed:
      return "CellularModemStateFailed";
    case kModemStateUnknown:
      return "CellularModemStateUnknown";
    case kModemStateInitializing:
      return "CellularModemStateInitializing";
    case kModemStateLocked:
      return "CellularModemStateLocked";
    case kModemStateDisabled:
      return "CellularModemStateDisabled";
    case kModemStateDisabling:
      return "CellularModemStateDisabling";
    case kModemStateEnabling:
      return "CellularModemStateEnabling";
    case kModemStateEnabled:
      return "CellularModemStateEnabled";
    case kModemStateSearching:
      return "CellularModemStateSearching";
    case kModemStateRegistered:
      return "CellularModemStateRegistered";
    case kModemStateDisconnecting:
      return "CellularModemStateDisconnecting";
    case kModemStateConnecting:
      return "CellularModemStateConnecting";
    case kModemStateConnected:
      return "CellularModemStateConnected";
    default:
      NOTREACHED();
  }
  return StringPrintf("CellularModemStateUnknown-%d", modem_state);
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
    bool(Cellular::*set)(const bool &value, Error *error)) {
  mutable_store()->RegisterDerivedBool(
      name,
      BoolAccessor(
          new CustomAccessor<Cellular, bool>(this, get, set)));
}

void Cellular::HelpRegisterConstDerivedString(
    const string &name,
    string(Cellular::*get)(Error *)) {
  mutable_store()->RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Cellular, string>(this, get, NULL)));
}

void Cellular::Start(Error *error,
                     const EnabledStateChangedCallback &callback) {
  DCHECK(error);
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  if (state_ != kStateDisabled) {
    return;
  }
  ResultCallback cb = Bind(&Cellular::StartModemCallback,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  capability_->StartModem(error, cb);
}

void Cellular::Stop(Error *error,
                    const EnabledStateChangedCallback &callback) {
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  explicit_disconnect_ = true;
  ResultCallback cb = Bind(&Cellular::StopModemCallback,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  capability_->StopModem(error, cb);
}

bool Cellular::IsUnderlyingDeviceEnabled() const {
  return IsEnabledModemState(modem_state_);
}

bool Cellular::IsModemRegistered() const {
  return (modem_state_ == Cellular::kModemStateRegistered ||
          modem_state_ == Cellular::kModemStateConnecting ||
          modem_state_ == Cellular::kModemStateConnected);
}

// static
bool Cellular::IsEnabledModemState(ModemState state) {
  switch (state) {
    case kModemStateFailed:
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

void Cellular::StartModemCallback(const EnabledStateChangedCallback &callback,
                                  const Error &error) {
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  if (error.IsSuccess() && (state_ == kStateDisabled)) {
    SetState(kStateEnabled);
    // Registration state updates may have been ignored while the
    // modem was not yet marked enabled.
    HandleNewRegistrationState();
    if (capability_->ShouldDetectOutOfCredit())
      set_traffic_monitor_enabled(true);
  }
  callback.Run(error);
}

void Cellular::StopModemCallback(const EnabledStateChangedCallback &callback,
                                 const Error &error) {
  SLOG(Cellular, 2) << __func__ << ": " << GetStateString(state_);
  explicit_disconnect_ = false;
  // Destroy the cellular service regardless of any errors that occur during
  // the stop process since we do not know the state of the modem at this
  // point.
  DestroyService();
  if (state_ != kStateDisabled)
    SetState(kStateDisabled);
  set_traffic_monitor_enabled(false);
  callback.Run(error);
  // In case no termination action was executed (and TerminationActionComplete
  // was not invoked) in response to a suspend request, any registered
  // termination action needs to be removed explicitly.
  manager()->RemoveTerminationAction(FriendlyName());
}

void Cellular::InitCapability(Type type) {
  // TODO(petkov): Consider moving capability construction into a factory that's
  // external to the Cellular class.
  SLOG(Cellular, 2) << __func__ << "(" << type << ")";
  switch (type) {
    case kTypeGSM:
      capability_.reset(new CellularCapabilityGSM(this,
                                                  proxy_factory_,
                                                  modem_info_));
      break;
    case kTypeCDMA:
      capability_.reset(new CellularCapabilityCDMA(this,
                                                   proxy_factory_,
                                                   modem_info_));
      break;
    case kTypeUniversal:
      capability_.reset(new CellularCapabilityUniversal(
          this,
          proxy_factory_,
          modem_info_));
      break;
    case kTypeUniversalCDMA:
      capability_.reset(new CellularCapabilityUniversalCDMA(
          this,
          proxy_factory_,
          modem_info_));
      break;
    default: NOTREACHED();
  }
}

void Cellular::Activate(const string &carrier,
                        Error *error, const ResultCallback &callback) {
  capability_->Activate(carrier, error, callback);
}

void Cellular::CompleteActivation(Error *error) {
  capability_->CompleteActivation(error);
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

void Cellular::Reset(Error *error, const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  capability_->Reset(error, callback);
}

void Cellular::SetCarrier(const string &carrier,
                          Error *error, const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << "(" << carrier << ")";
  capability_->SetCarrier(carrier, error, callback);
}

void Cellular::DropConnection() {
  if (ppp_device_) {
    // For PPP dongles, IP configuration is handled on the |ppp_device_|,
    // rather than the netdev plumbed into |this|.
    ppp_device_->DropConnection();
  } else {
    Device::DropConnection();
  }
}

void Cellular::SetServiceState(Service::ConnectState state) {
  if (ppp_device_) {
    ppp_device_->SetServiceState(state);
  } else if (selected_service()) {
    Device::SetServiceState(state);
  } else if (service_) {
    service_->SetState(state);
  } else {
    LOG(WARNING) << "State change with no Service.";
  }
}

void Cellular::SetServiceFailure(Service::ConnectFailure failure_state) {
  if (ppp_device_) {
    ppp_device_->SetServiceFailure(failure_state);
  } else if (selected_service()) {
    Device::SetServiceFailure(failure_state);
  } else if (service_) {
    service_->SetFailure(failure_state);
  } else {
    LOG(WARNING) << "State change with no Service.";
  }
}

void Cellular::SetServiceFailureSilent(Service::ConnectFailure failure_state) {
  if (ppp_device_) {
    ppp_device_->SetServiceFailureSilent(failure_state);
  } else if (selected_service()) {
    Device::SetServiceFailureSilent(failure_state);
  } else if (service_) {
    service_->SetFailureSilent(failure_state);
  } else {
    LOG(WARNING) << "State change with no Service.";
  }
}

void Cellular::OnNoNetworkRouting() {
  SLOG(Cellular, 2) << __func__;
  Device::OnNoNetworkRouting();

  SLOG(Cellular, 2) << "Requesting active probe for out-of-credit detection.";
  RequestConnectionHealthCheck();
}

void Cellular::OnAfterResume() {
  SLOG(Cellular, 2) << __func__;
  if (enabled_persistent()) {
    LOG(INFO) << "Restarting modem after resume.";

    // If we started disabling the modem before suspend, but that
    // suspend is still in progress, then we are not yet in
    // kStateDisabled. That's a problem, because Cellular::Start
    // returns immediately in that case. Hack around that by forcing
    // |state_| here.
    //
    // TODO(quiche): Remove this hack. Maybe
    // CellularCapabilityUniversal should generate separate
    // notifications for Stop_Disable, and Stop_PowerDown. Then we'd
    // update our state to kStateDisabled when Stop_Disable completes.
    state_ = kStateDisabled;

    Error error;
    SetEnabledUnchecked(true, &error, Bind(LogRestartModemResult));
    if (error.IsSuccess()) {
      LOG(INFO) << "Modem restart completed immediately.";
    } else if (error.IsOngoing()) {
      LOG(INFO) << "Modem restart in progress.";
    } else {
      LOG(WARNING) << "Modem restart failed: " << error;
    }
  }
  // TODO(quiche): Consider if this should be conditional. If, e.g.,
  // the device was still disabling when we suspended, will trying to
  // renew DHCP here cause problems?
  Device::OnAfterResume();
}

void Cellular::OnConnectionHealthCheckerResult(
      ConnectionHealthChecker::Result result) {
  SLOG(Cellular, 2) << __func__ << "(Result = "
                    << ConnectionHealthChecker::ResultToString(result) << ")";

  if (result == ConnectionHealthChecker::kResultCongestedTxQueue) {
    SLOG(Cellular, 2) << "Active probe determined possible out-of-credits "
                      << "scenario.";
    if (service().get()) {
      Metrics::CellularOutOfCreditsReason reason =
          (result == ConnectionHealthChecker::kResultCongestedTxQueue) ?
              Metrics::kCellularOutOfCreditsReasonTxCongested :
              Metrics::kCellularOutOfCreditsReasonElongatedTimeWait;
      metrics()->NotifyCellularOutOfCredits(reason);

      service()->SetOutOfCredits(true);
      SLOG(Cellular, 2) << "Disconnecting due to out-of-credit scenario.";
      Error error;
      service()->Disconnect(&error);
    }
  }
}

void Cellular::PortalDetectorCallback(const PortalDetector::Result &result) {
  Device::PortalDetectorCallback(result);
  if (result.status != PortalDetector::kStatusSuccess &&
      capability_->ShouldDetectOutOfCredit()) {
    SLOG(Cellular, 2) << "Portal detection failed. Launching active probe for "
                      << "out-of-credit detection.";
    RequestConnectionHealthCheck();
  }
}

void Cellular::Scan(ScanType /*scan_type*/, Error *error,
                    const string &/*reason*/) {
  // |scan_type| is ignored because Cellular only does a full scan.
  // TODO(ers): for now report immediate success or failure.
  capability_->Scan(error, ResultCallback());
}

void Cellular::HandleNewRegistrationState() {
  SLOG(Cellular, 2) << __func__
                    << ": (new state " << GetStateString(state_) << ")";
  if (capability_->IsServiceActivationRequired()) {
    if (state_ == kStateEnabled && !service_.get()) {
      SLOG(Cellular, 2) << "Service activation required. Creating dummy "
                        << "service.";
      CreateService();
    }
    return;
  }
  if (!capability_->IsRegistered()) {
    if (!explicit_disconnect_ &&
        (state_ == kStateLinked || state_ == kStateConnected) &&
        service_.get())
      metrics()->NotifyCellularDeviceDrop(
          capability_->GetNetworkTechnologyString(), service_->strength());
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
    metrics()->NotifyDeviceScanFinished(interface_index());
    CreateService();
  }
  capability_->GetSignalQuality();
  if (state_ == kStateRegistered && modem_state_ == kModemStateConnected)
    OnConnected();
  service_->SetNetworkTechnology(capability_->GetNetworkTechnologyString());
  service_->SetRoamingState(capability_->GetRoamingStateString());
  manager()->UpdateService(service_);
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
  service_ = new CellularService(modem_info_, this);
  capability_->OnServiceCreated();
  manager()->RegisterService(service_);
}

void Cellular::DestroyService() {
  SLOG(Cellular, 2) << __func__;
  DropConnection();
  if (service_) {
    LOG(INFO) << "Deregistering cellular service " << service_->unique_name()
              << " for device " << link_name();
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
}

void Cellular::Connect(Error *error) {
  SLOG(Cellular, 2) << __func__;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    Error::PopulateAndLog(error, Error::kAlreadyConnected,
                          "Already connected; connection request ignored.");
    return;
  } else if (state_ != kStateRegistered) {
    Error::PopulateAndLog(error, Error::kNotRegistered,
                          "Modem not registered; connection request ignored.");
    return;
  }

  if (!capability_->AllowRoaming() &&
      service_->roaming_state() == kRoamingStateRoaming) {
    Error::PopulateAndLog(error, Error::kNotOnHomeNetwork,
                          "Roaming disallowed; connection request ignored.");
    return;
  }

  DBusPropertiesMap properties;
  capability_->SetupConnectProperties(&properties);
  ResultCallback cb = Bind(&Cellular::OnConnectReply,
                           weak_ptr_factory_.GetWeakPtr());
  OnConnecting();
  capability_->Connect(properties, error, cb);
  if (!error->IsSuccess())
    return;

  bool is_auto_connecting = service_.get() && service_->is_auto_connecting();
  metrics()->NotifyDeviceConnectStarted(interface_index(), is_auto_connecting);
}

// Note that there's no ResultCallback argument to this,
// since Connect() isn't yet passed one.
void Cellular::OnConnectReply(const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  if (error.IsSuccess()) {
    metrics()->NotifyDeviceConnectFinished(interface_index());
    OnConnected();
  } else {
    metrics()->NotifyCellularDeviceFailure(error);
    OnConnectFailed(error);
  }
}

void Cellular::OnDisabled() {
  SetEnabled(false);
}

void Cellular::OnEnabled() {
  manager()->AddTerminationAction(FriendlyName(),
                                  Bind(&Cellular::StartTermination,
                                       weak_ptr_factory_.GetWeakPtr()));
  SetEnabled(true);
}

void Cellular::OnConnecting() {
  if (service_)
    service_->SetState(Service::kStateAssociating);
}

void Cellular::OnConnected() {
  SLOG(Cellular, 2) << __func__;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    SLOG(Cellular, 2) << "Already connected";
    return;
  }
  SetState(kStateConnected);
  if (!service_) {
    LOG(INFO) << "Disconnecting due to no cellular service.";
    Disconnect(NULL);
  } else if (!capability_->AllowRoaming() &&
      service_->roaming_state() == kRoamingStateRoaming) {
    LOG(INFO) << "Disconnecting due to roaming.";
    Disconnect(NULL);
  } else {
    EstablishLink();
  }
}

void Cellular::OnConnectFailed(const Error &error) {
  if (service_)
    service_->SetFailure(Service::kFailureUnknown);
}

void Cellular::Disconnect(Error *error) {
  SLOG(Cellular, 2) << __func__;
  if (state_ != kStateConnected && state_ != kStateLinked) {
    Error::PopulateAndLog(
        error, Error::kNotConnected, "Not connected; request ignored.");
    return;
  }
  StopPPP();
  explicit_disconnect_ = true;
  ResultCallback cb = Bind(&Cellular::OnDisconnectReply,
                           weak_ptr_factory_.GetWeakPtr());
  capability_->Disconnect(error, cb);
}

void Cellular::OnDisconnectReply(const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  explicit_disconnect_ = false;
  if (error.IsSuccess()) {
    OnDisconnected();
  } else {
    metrics()->NotifyCellularDeviceFailure(error);
    OnDisconnectFailed();
  }
}

void Cellular::OnDisconnected() {
  SLOG(Cellular, 2) << __func__;
  if (!DisconnectCleanup()) {
    LOG(WARNING) << "Disconnect occurred while in state "
                 << GetStateString(state_);
  }
}

void Cellular::OnDisconnectFailed() {
  SLOG(Cellular, 2) << __func__;
  // If the modem is in the disconnecting state, then
  // the disconnect should eventually succeed, so do
  // nothing.
  if (modem_state_ == kModemStateDisconnecting) {
    LOG(WARNING) << "Ignoring failed disconnect while modem is disconnecting.";
    return;
  }

  // OnDisconnectFailed got called because no bearers
  // to disconnect were found. Which means that we shouldn't
  // really remain in the connected/linked state if we
  // are in one of those.
  if (!DisconnectCleanup()) {
    // otherwise, no-op
    LOG(WARNING) << "Ignoring failed disconnect while in state "
                 << GetStateString(state_);
  }

  // TODO(armansito): In either case, shill ends up thinking
  // that it's disconnected, while for some reason the underlying
  // modem might still actually be connected. In that case the UI
  // would be reflecting an incorrect state and a further connection
  // request would fail. We should perhaps tear down the modem and
  // restart it here.
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

  // Set state to associating.
  OnConnecting();
}

void Cellular::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if (ppp_task_) {
    LOG(INFO) << "Ignoring LinkEvent on device with PPP interface.";
    return;
  }

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
    LOG(INFO) << link_name() << " is down.";
    SetState(kStateConnected);
    DropConnection();
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

void Cellular::OnModemStateChanged(ModemState new_state) {
  ModemState old_state = modem_state_;
  SLOG(Cellular, 2) << __func__ << ": " << GetModemStateString(old_state)
                    << " -> " << GetModemStateString(new_state);
  if (old_state == new_state) {
    SLOG(Cellular, 2) << "The new state matches the old state. Nothing to do.";
    return;
  }
  set_modem_state(new_state);
  if (old_state >= kModemStateRegistered &&
      new_state < kModemStateRegistered) {
    capability_->SetUnregistered(new_state == kModemStateSearching);
    HandleNewRegistrationState();
  }
  if (new_state == kModemStateDisabled) {
    OnDisabled();
  } else if (new_state >= kModemStateEnabled) {
    if (old_state < kModemStateEnabled) {
      // Just became enabled, update enabled state.
      OnEnabled();
    }
    if ((new_state == kModemStateEnabled ||
         new_state == kModemStateSearching ||
         new_state == kModemStateRegistered) &&
        (old_state == kModemStateConnected ||
         old_state == kModemStateConnecting ||
         old_state == kModemStateDisconnecting))
      OnDisconnected();
    else if (new_state == kModemStateConnecting)
      OnConnecting();
    else if (new_state == kModemStateConnected &&
             old_state == kModemStateConnecting)
      OnConnected();
  }
}

bool Cellular::IsActivating() const {
  return capability_->IsActivating();
}

bool Cellular::SetAllowRoaming(const bool &value, Error */*error*/) {
  SLOG(Cellular, 2) << __func__
                    << "(" << allow_roaming_ << "->" << value << ")";
  if (allow_roaming_ == value) {
    return false;
  }
  allow_roaming_ = value;
  manager()->UpdateDevice(this);

  // Use AllowRoaming() instead of allow_roaming_ in order to
  // incorporate provider preferences when evaluating if a disconnect
  // is required.
  if (!capability_->AllowRoaming() &&
      capability_->GetRoamingStateString() == kRoamingStateRoaming) {
    Error error;
    Disconnect(&error);
  }
  adaptor()->EmitBoolChanged(kCellularAllowRoamingProperty, value);
  return true;
}

void Cellular::StartTermination() {
  LOG(INFO) << __func__;
  Error error;
  StopPPP();
  SetEnabledNonPersistent(
      false,
      &error,
      Bind(&Cellular::OnTerminationCompleted, weak_ptr_factory_.GetWeakPtr()));
  if (error.IsFailure() && error.type() != Error::kInProgress) {
    // If we fail to disable the modem right away, proceed to suspend instead of
    // wasting the time to wait for the suspend delay to expire.
    LOG(WARNING)
        << "Proceed to suspend even through the modem is not yet disabled: "
        << error;
    OnTerminationCompleted(error);
  }
}

void Cellular::OnTerminationCompleted(const Error &error) {
  LOG(INFO) << __func__ << ": " << error;
  manager()->TerminationActionComplete(FriendlyName());
  manager()->RemoveTerminationAction(FriendlyName());
}

bool Cellular::DisconnectCleanup() {
  bool succeeded = false;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    SetState(kStateRegistered);
    SetServiceFailureSilent(Service::kFailureUnknown);
    DestroyIPConfig();
    succeeded = true;
  }
  capability_->DisconnectCleanup();
  return succeeded;
}

// static
void Cellular::LogRestartModemResult(const Error &error) {
  if (error.IsSuccess()) {
    LOG(INFO) << "Modem restart completed.";
  } else {
    LOG(WARNING) << "Attempt to restart modem failed: " << error;
  }
}

void Cellular::StartPPP(const string &serial_device) {
  SLOG(PPP, 2) << __func__ << " on " << serial_device;
  // Detach any SelectedService from this device. It will be grafted onto
  // the PPPDevice after PPP is up (in Cellular::Notify).
  //
  // This has two important effects: 1) kills dhcpcd if it is running.
  // 2) stops Cellular::LinkEvent from driving changes to the
  // SelectedService.
  if (selected_service()) {
    CHECK_EQ(service_.get(), selected_service().get());
    // Save and restore |service_| state, as DropConnection calls
    // SelectService, and SelectService will move selected_service()
    // to kStateIdle.
    Service::ConnectState original_state(service_->state());
    Device::DropConnection();  // Don't redirect to PPPDevice.
    service_->SetState(original_state);
  } else {
    CHECK(!ipconfig());  // Shouldn't have ipconfig without selected_service().
  }

  base::Callback<void(pid_t, int)> death_callback(
      Bind(&Cellular::OnPPPDied, weak_ptr_factory_.GetWeakPtr()));
  vector<string> args;
  map<string, string> environment;
  Error error;
  if (SLOG_IS_ON(PPP, 5)) {
    args.push_back("debug");
  }
  args.push_back("nodetach");
  args.push_back("nodefaultroute");  // Don't let pppd muck with routing table.
  args.push_back("usepeerdns");  // Request DNS servers.
  args.push_back("plugin");  // Goes with next arg.
  args.push_back(PPPDevice::kPluginPath);
  args.push_back(serial_device);
  is_ppp_authenticating_ = false;
  scoped_ptr<ExternalTask> new_ppp_task(
      new ExternalTask(modem_info_->control_interface(),
                       modem_info_->glib(),
                       weak_ptr_factory_.GetWeakPtr(),
                       death_callback));
  if (new_ppp_task->Start(
          FilePath(PPPDevice::kDaemonPath), args, environment, true, &error)) {
    LOG(INFO) << "Forked pppd process.";
    ppp_task_ = new_ppp_task.Pass();
  }
}

void Cellular::StopPPP() {
  SLOG(PPP, 2) << __func__;
  if (!ppp_task_) {
    return;
  }
  DropConnection();
  ppp_task_.reset();
  ppp_device_ = NULL;
}

// called by |ppp_task_|
void Cellular::GetLogin(string *user, string *password) {
  SLOG(PPP, 2) << __func__;
  if (!service()) {
    LOG(ERROR) << __func__ << " with no service ";
    return;
  }
  CHECK(user);
  CHECK(password);
  *user = service()->ppp_username();
  *password = service()->ppp_password();
}

// Called by |ppp_task_|.
void Cellular::Notify(const string &reason,
                      const map<string, string> &dict) {
  SLOG(PPP, 2) << __func__ << " " << reason << " on " << link_name();

  if (reason == kPPPReasonAuthenticating) {
    OnPPPAuthenticating();
  } else if (reason == kPPPReasonAuthenticated) {
    OnPPPAuthenticated();
  } else if (reason == kPPPReasonConnect) {
    OnPPPConnected(dict);
  } else if (reason == kPPPReasonDisconnect) {
    OnPPPDisconnected();
  } else {
    NOTREACHED();
  }
}

void Cellular::OnPPPAuthenticated() {
  SLOG(PPP, 2) << __func__;
  is_ppp_authenticating_ = false;
}

void Cellular::OnPPPAuthenticating() {
  SLOG(PPP, 2) << __func__;
  is_ppp_authenticating_ = true;
}

void Cellular::OnPPPConnected(const map<string, string> &params) {
  SLOG(PPP, 2) << __func__;
  string interface_name = PPPDevice::GetInterfaceName(params);
  DeviceInfo *device_info = modem_info_->manager()->device_info();
  int interface_index = device_info->GetIndex(interface_name);
  if (interface_index < 0) {
    // TODO(quiche): Consider handling the race when the RTNL notification about
    // the new PPP device has not been received yet. crbug.com/246832.
    NOTIMPLEMENTED() << ": No device info for " << interface_name << ".";
    return;
  }

  if (!ppp_device_ || ppp_device_->interface_index() != interface_index) {
    if (ppp_device_) {
      ppp_device_->SelectService(NULL);  // No longer drives |service_|.
    }
    ppp_device_ = ppp_device_factory_->CreatePPPDevice(
        modem_info_->control_interface(),
        modem_info_->dispatcher(),
        modem_info_->metrics(),
        modem_info_->manager(),
        interface_name,
        interface_index);
    device_info->RegisterDevice(ppp_device_);
  }

  CHECK(service_);
  // For PPP, we only SelectService on the |ppp_device_|.
  CHECK(!selected_service());
  const bool kBlackholeIPv6 = false;
  ppp_device_->SetEnabled(true);
  ppp_device_->SelectService(service_);
  ppp_device_->UpdateIPConfigFromPPP(params, kBlackholeIPv6);
}

void Cellular::OnPPPDisconnected() {
  SLOG(PPP, 2) << __func__;
  // DestroyLater, rather than while on stack.
  ppp_task_.release()->DestroyLater(modem_info_->dispatcher());
  if (is_ppp_authenticating_) {
    SetServiceFailure(Service::kFailurePPPAuth);
  } else {
    // TODO(quiche): Don't set failure if we disconnected intentionally.
    SetServiceFailure(Service::kFailureUnknown);
  }
  Error error;
  Disconnect(&error);
}

void Cellular::OnPPPDied(pid_t pid, int exit) {
  LOG(INFO) << __func__ << " on " << link_name();
  OnPPPDisconnected();
}

}  // namespace shill
