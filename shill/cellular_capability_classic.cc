// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_classic.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Callback;
using base::Closure;
using std::string;

namespace shill {

const char CellularCapabilityClassic::kConnectPropertyApn[] = "apn";
const char CellularCapabilityClassic::kConnectPropertyApnUsername[] =
    "username";
const char CellularCapabilityClassic::kConnectPropertyApnPassword[] =
    "password";
const char CellularCapabilityClassic::kConnectPropertyHomeOnly[] = "home_only";
const char CellularCapabilityClassic::kConnectPropertyPhoneNumber[] = "number";
const char CellularCapabilityClassic::kModemPropertyEnabled[] = "Enabled";

static Cellular::ModemState ConvertClassicToModemState(uint32 classic_state) {
  ModemClassicState cstate =
      static_cast<ModemClassicState>(classic_state);
  switch (cstate) {
    case kModemClassicStateUnknown:
      return Cellular::kModemStateUnknown;
    case kModemClassicStateDisabled:
      return Cellular::kModemStateDisabled;
    case kModemClassicStateDisabling:
      return Cellular::kModemStateDisabling;
    case kModemClassicStateEnabling:
      return Cellular::kModemStateEnabling;
    case kModemClassicStateEnabled:
      return Cellular::kModemStateEnabled;
    case kModemClassicStateSearching:
      return Cellular::kModemStateSearching;
    case kModemClassicStateRegistered:
      return Cellular::kModemStateRegistered;
    case kModemClassicStateDisconnecting:
      return Cellular::kModemStateDisconnecting;
    case kModemClassicStateConnecting:
      return Cellular::kModemStateConnecting;
    case kModemClassicStateConnected:
      return Cellular::kModemStateConnected;
    default:
      return Cellular::kModemStateUnknown;
  };
}

CellularCapabilityClassic::CellularCapabilityClassic(
    Cellular *cellular,
    ProxyFactory *proxy_factory)
    : CellularCapability(cellular, proxy_factory),
      scanning_supported_(false),
      weak_ptr_factory_(this) {
  PropertyStore *store = cellular->mutable_store();
  store->RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  store->RegisterConstBool(flimflam::kSupportNetworkScanProperty,
                           &scanning_supported_);
  store->RegisterConstString(flimflam::kEsnProperty, &esn_);
  store->RegisterConstString(flimflam::kFirmwareRevisionProperty,
                             &firmware_revision_);
  store->RegisterConstString(flimflam::kHardwareRevisionProperty,
                             &hardware_revision_);
  store->RegisterConstString(flimflam::kImeiProperty, &imei_);
  store->RegisterConstString(flimflam::kImsiProperty, &imsi_);
  store->RegisterConstString(flimflam::kManufacturerProperty, &manufacturer_);
  store->RegisterConstString(flimflam::kMdnProperty, &mdn_);
  store->RegisterConstString(flimflam::kMeidProperty, &meid_);
  store->RegisterConstString(flimflam::kMinProperty, &min_);
  store->RegisterConstString(flimflam::kModelIDProperty, &model_id_);
}

CellularCapabilityClassic::~CellularCapabilityClassic() {}

void CellularCapabilityClassic::InitProxies() {
  SLOG(Cellular, 2) << __func__;
  proxy_.reset(proxy_factory()->CreateModemProxy(
      cellular()->dbus_path(), cellular()->dbus_owner()));
  simple_proxy_.reset(proxy_factory()->CreateModemSimpleProxy(
      cellular()->dbus_path(), cellular()->dbus_owner()));
  proxy_->set_state_changed_callback(
      Bind(&CellularCapabilityClassic::OnModemStateChangedSignal,
           weak_ptr_factory_.GetWeakPtr()));
}

void CellularCapabilityClassic::ReleaseProxies() {
  SLOG(Cellular, 2) << __func__;
  proxy_.reset();
  simple_proxy_.reset();
}

void CellularCapabilityClassic::FinishEnable(const ResultCallback &callback) {
  // Normally, running the callback is the last thing done in a method.
  // In this case, we do it first, because we want to make sure that
  // the device is marked as Enabled before the registration state is
  // handled. See comment in Cellular::HandleNewRegistrationState.
  callback.Run(Error());
  GetRegistrationState();
  GetSignalQuality();
}

void CellularCapabilityClassic::FinishDisable(const ResultCallback &callback) {
  ReleaseProxies();
  callback.Run(Error());
}

void CellularCapabilityClassic::OnUnsupportedOperation(
    const char *operation,
    Error *error) {
  string message("The ");
  message.append(operation).append(" operation is not supported.");
  Error::PopulateAndLog(error, Error::kNotSupported, message);
}

void CellularCapabilityClassic::RunNextStep(CellularTaskList *tasks) {
  CHECK(!tasks->empty());
  SLOG(Cellular, 2) << __func__ << ": " << tasks->size() << " remaining tasks";
  Closure task = (*tasks)[0];
  tasks->erase(tasks->begin());
  cellular()->dispatcher()->PostTask(task);
}

void CellularCapabilityClassic::StepCompletedCallback(
    const ResultCallback &callback,
    bool ignore_error,
    CellularTaskList *tasks,
    const Error &error) {
  if ((ignore_error || error.IsSuccess()) && !tasks->empty()) {
    RunNextStep(tasks);
    return;
  }
  delete tasks;
  if (!callback.is_null())
    callback.Run(error);
}

// always called from an async context
void CellularCapabilityClassic::EnableModem(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  proxy_->Enable(true, &error, callback, kTimeoutEnable);
  if (error.IsFailure())
    callback.Run(error);
}

// always called from an async context
void CellularCapabilityClassic::DisableModem(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  proxy_->Enable(false, &error, callback, kTimeoutEnable);
  if (error.IsFailure())
      callback.Run(error);
}

// always called from an async context
void CellularCapabilityClassic::GetModemStatus(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  DBusPropertyMapCallback cb = Bind(
      &CellularCapabilityClassic::OnGetModemStatusReply,
      weak_ptr_factory_.GetWeakPtr(), callback);
  Error error;
  simple_proxy_->GetModemStatus(&error, cb, kTimeoutDefault);
  if (error.IsFailure())
      callback.Run(error);
}

// always called from an async context
void CellularCapabilityClassic::GetModemInfo(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  ModemInfoCallback cb = Bind(&CellularCapabilityClassic::OnGetModemInfoReply,
                              weak_ptr_factory_.GetWeakPtr(), callback);
  Error error;
  proxy_->GetModemInfo(&error, cb, kTimeoutDefault);
  if (error.IsFailure())
      callback.Run(error);
}

void CellularCapabilityClassic::StopModem(Error *error,
                                   const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;

  CellularTaskList *tasks = new CellularTaskList();
  ResultCallback cb =
      Bind(&CellularCapabilityClassic::StepCompletedCallback,
           weak_ptr_factory_.GetWeakPtr(), callback, false, tasks);
  ResultCallback cb_ignore_error =
      Bind(&CellularCapabilityClassic::StepCompletedCallback,
           weak_ptr_factory_.GetWeakPtr(), callback, true, tasks);
  // TODO(ers): We can skip the call to Disconnect if the modem has
  // told us that the modem state is Disabled or Registered.
  tasks->push_back(Bind(&CellularCapabilityClassic::Disconnect,
                        weak_ptr_factory_.GetWeakPtr(),
                        static_cast<Error *>(NULL), cb_ignore_error));
  // TODO(ers): We can skip the call to Disable if the modem has
  // told us that the modem state is Disabled.
  tasks->push_back(Bind(&CellularCapabilityClassic::DisableModem,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityClassic::FinishDisable,
                        weak_ptr_factory_.GetWeakPtr(), cb));

  RunNextStep(tasks);
}

void CellularCapabilityClassic::Connect(const DBusPropertiesMap &properties,
                                 Error *error,
                                 const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  simple_proxy_->Connect(properties, error, callback, kTimeoutConnect);
}

void CellularCapabilityClassic::Disconnect(Error *error,
                                    const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  if (proxy_.get())
    proxy_->Disconnect(error, callback, kTimeoutDefault);
}

void CellularCapabilityClassic::Activate(const string &/*carrier*/,
                                  Error *error,
                                  const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityClassic::RegisterOnNetwork(
    const string &/*network_id*/,
    Error *error, const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityClassic::RequirePIN(const std::string &/*pin*/,
                                    bool /*require*/,
                                    Error *error,
                                    const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityClassic::EnterPIN(const string &/*pin*/,
                                  Error *error,
                                  const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityClassic::UnblockPIN(const string &/*unblock_code*/,
                                    const string &/*pin*/,
                                    Error *error,
                                    const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityClassic::ChangePIN(const string &/*old_pin*/,
                                   const string &/*new_pin*/,
                                   Error *error,
                                   const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityClassic::Scan(Error *error,
                              const ResultCallback &callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityClassic::OnDBusPropertiesChanged(
    const std::string &interface,
    const DBusPropertiesMap &changed_properties,
    const std::vector<std::string> &invalidated_properties) {
  bool enabled;
  // This solves a bootstrapping problem: If the modem is not yet
  // enabled, there are no proxy objects associated with the capability
  // object, so modem signals like StateChanged aren't seen. By monitoring
  // changes to the Enabled property via the ModemManager, we're able to
  // get the initialization process started, which will result in the
  // creation of the proxy objects.
  //
  // The first time we see the change to Enabled (when the modem state
  // is Unknown), we simply update the state, and rely on the Manager to
  // enable the device when it is registered with the Manager. On subsequent
  // changes to Enabled, we need to explicitly enable the device ourselves.
  if (DBusProperties::GetBool(changed_properties,
                              kModemPropertyEnabled, &enabled)) {
    Cellular::ModemState prev_modem_state = cellular()->modem_state();
    if (enabled)
      cellular()->set_modem_state(Cellular::kModemStateEnabled);
    else
      cellular()->set_modem_state(Cellular::kModemStateDisabled);
    if (enabled && cellular()->state() == Cellular::kStateDisabled &&
        prev_modem_state != Cellular::kModemStateUnknown &&
        prev_modem_state != Cellular::kModemStateEnabled) {
      cellular()->SetEnabled(true);
    }
  }
}

void CellularCapabilityClassic::OnGetModemStatusReply(
    const ResultCallback &callback,
    const DBusPropertiesMap &props,
    const Error &error) {
  SLOG(Cellular, 2) << __func__ << " " << props.size() << " props. error "
                    << error;
  if (error.IsSuccess()) {
    DBusProperties::GetString(props, "carrier", &carrier_);
    DBusProperties::GetString(props, "meid", &meid_);
    DBusProperties::GetString(props, "imei", &imei_);
    DBusProperties::GetString(props, kModemPropertyIMSI, &imsi_);
    DBusProperties::GetString(props, "esn", &esn_);
    DBusProperties::GetString(props, "mdn", &mdn_);
    DBusProperties::GetString(props, "min", &min_);
    DBusProperties::GetString(props, "firmware_revision", &firmware_revision_);
    UpdateStatus(props);
  }
  callback.Run(error);
}

void CellularCapabilityClassic::OnGetModemInfoReply(
    const ResultCallback &callback,
    const ModemHardwareInfo &info,
    const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  if (error.IsSuccess()) {
    manufacturer_ = info._1;
    model_id_ = info._2;
    hardware_revision_ = info._3;
    SLOG(Cellular, 2) << __func__ << ": " << info._1 << ", " << info._2 << ", "
                      << info._3;
  }
  callback.Run(error);
}

void CellularCapabilityClassic::OnModemStateChangedSignal(
    uint32 old_state, uint32 new_state, uint32 reason) {
  SLOG(Cellular, 2) << __func__ << "(" << old_state << ", " << new_state << ", "
                    << reason << ")";
  cellular()->OnModemStateChanged(ConvertClassicToModemState(old_state),
                                  ConvertClassicToModemState(new_state),
                                  reason);
}

}  // namespace shill
