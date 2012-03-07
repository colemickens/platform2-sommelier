// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Callback;
using base::Closure;
using std::string;

namespace shill {

const char CellularCapability::kConnectPropertyPhoneNumber[] = "number";
const char CellularCapability::kPropertyIMSI[] = "imsi";
// All timeout values are in milliseconds
const int CellularCapability::kTimeoutActivate = 120000;
const int CellularCapability::kTimeoutConnect = 45000;
const int CellularCapability::kTimeoutDefault = 5000;
const int CellularCapability::kTimeoutEnable = 15000;
const int CellularCapability::kTimeoutRegister = 90000;
const int CellularCapability::kTimeoutScan = 120000;

CellularCapability::CellularCapability(Cellular *cellular,
                                       ProxyFactory *proxy_factory)
    : allow_roaming_(false),
      scanning_supported_(false),
      cellular_(cellular),
      proxy_factory_(proxy_factory),
      weak_ptr_factory_(this) {
  PropertyStore *store = cellular->mutable_store();
  store->RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  HelpRegisterDerivedBool(flimflam::kCellularAllowRoamingProperty,
                          &CellularCapability::GetAllowRoaming,
                          &CellularCapability::SetAllowRoaming);
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

CellularCapability::~CellularCapability() {}

void CellularCapability::HelpRegisterDerivedBool(
    const string &name,
    bool(CellularCapability::*get)(Error *error),
    void(CellularCapability::*set)(const bool &value, Error *error)) {
  cellular()->mutable_store()->RegisterDerivedBool(
      name,
      BoolAccessor(
          new CustomAccessor<CellularCapability, bool>(this, get, set)));
}

void CellularCapability::SetAllowRoaming(const bool &value, Error */*error*/) {
  VLOG(2) << __func__ << "(" << allow_roaming_ << "->" << value << ")";
  if (allow_roaming_ == value) {
    return;
  }
  allow_roaming_ = value;
  if (!value && GetRoamingStateString() == flimflam::kRoamingStateRoaming) {
    Error error;
    cellular()->Disconnect(&error);
  }
  cellular()->adaptor()->EmitBoolChanged(
      flimflam::kCellularAllowRoamingProperty, value);
}

void CellularCapability::RunNextStep(CellularTaskList *tasks) {
  CHECK(!tasks->empty());
  VLOG(2) << __func__ << ": " << tasks->size() << " remaining tasks";
  Closure task = (*tasks)[0];
  tasks->erase(tasks->begin());
  cellular()->dispatcher()->PostTask(task);
}

void CellularCapability::StepCompletedCallback(
    const ResultCallback &callback,
    CellularTaskList *tasks,
    const Error &error) {
  if (error.IsSuccess() && !tasks->empty()) {
    RunNextStep(tasks);
    return;
  }
  delete tasks;
  callback.Run(error);
}

void CellularCapability::InitProxies() {
  VLOG(2) << __func__;
  proxy_.reset(proxy_factory()->CreateModemProxy(
      cellular()->dbus_path(), cellular()->dbus_owner()));
  simple_proxy_.reset(proxy_factory()->CreateModemSimpleProxy(
      cellular()->dbus_path(), cellular()->dbus_owner()));
  proxy_->set_state_changed_callback(
      Bind(&CellularCapability::OnModemStateChangedSignal,
           weak_ptr_factory_.GetWeakPtr()));
}

void CellularCapability::ReleaseProxies() {
  VLOG(2) << __func__;
  proxy_.reset();
  simple_proxy_.reset();
}

void CellularCapability::FinishEnable(const ResultCallback &callback) {
  callback.Run(Error());
  GetRegistrationState();
  GetSignalQuality();
}

void CellularCapability::FinishDisable(const ResultCallback &callback) {
  ReleaseProxies();
  callback.Run(Error());
}

void CellularCapability::OnUnsupportedOperation(
    const char *operation,
    Error *error) {
  string message("The ");
  message.append(operation).append(" operation is not supported.");
  Error::PopulateAndLog(error, Error::kNotSupported, message);
}

// always called from an async context
void CellularCapability::EnableModem(const ResultCallback &callback) {
  VLOG(2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  proxy_->Enable(true, &error, callback, kTimeoutEnable);
  if (error.IsFailure())
    callback.Run(error);
}

// always called from an async context
void CellularCapability::DisableModem(const ResultCallback &callback) {
  VLOG(2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  proxy_->Enable(false, &error, callback, kTimeoutDefault);
  if (error.IsFailure())
      callback.Run(error);
}

// always called from an async context
void CellularCapability::GetModemStatus(const ResultCallback &callback) {
  VLOG(2) << __func__;
  CHECK(!callback.is_null());
  DBusPropertyMapCallback cb = Bind(&CellularCapability::OnGetModemStatusReply,
                                    weak_ptr_factory_.GetWeakPtr(), callback);
  Error error;
  simple_proxy_->GetModemStatus(&error, cb, kTimeoutDefault);
  if (error.IsFailure())
      callback.Run(error);
}

// always called from an async context
void CellularCapability::GetModemInfo(const ResultCallback &callback) {
  VLOG(2) << __func__;
  CHECK(!callback.is_null());
  ModemInfoCallback cb = Bind(&CellularCapability::OnGetModemInfoReply,
                              weak_ptr_factory_.GetWeakPtr(), callback);
  Error error;
  proxy_->GetModemInfo(&error, cb, kTimeoutDefault);
  if (error.IsFailure())
      callback.Run(error);
}

void CellularCapability::Connect(const DBusPropertiesMap &properties,
                                 Error *error,
                                 const ResultCallback &callback) {
  VLOG(2) << __func__;
  ResultCallback cb = Bind(&CellularCapability::OnConnectReply,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  simple_proxy_->Connect(properties, error, cb, kTimeoutConnect);
}

void CellularCapability::Disconnect(Error *error,
                                    const ResultCallback &callback) {
  VLOG(2) << __func__;
  ResultCallback cb = Bind(&CellularCapability::OnDisconnectReply,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  proxy_->Disconnect(error, cb, kTimeoutDefault);
}

void CellularCapability::Activate(const string &/*carrier*/,
                                  Error *error,
                                  const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::RegisterOnNetwork(
    const string &/*network_id*/,
    Error *error, const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::RequirePIN(const std::string &/*pin*/,
                                    bool /*require*/,
                                    Error *error,
                                    const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::EnterPIN(const string &/*pin*/,
                                  Error *error,
                                  const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::UnblockPIN(const string &/*unblock_code*/,
                                    const string &/*pin*/,
                                    Error *error,
                                    const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::ChangePIN(const string &/*old_pin*/,
                                   const string &/*new_pin*/,
                                   Error *error,
                                   const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::Scan(Error *error,
                              const ResultCallback &callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::OnGetModemStatusReply(
    const ResultCallback &callback,
    const DBusPropertiesMap &props,
    const Error &error) {
  VLOG(2) << __func__ << " " << props.size() << " props. error " << error;
  if (error.IsSuccess()) {
    DBusProperties::GetString(props, "carrier", &carrier_);
    DBusProperties::GetString(props, "meid", &meid_);
    DBusProperties::GetString(props, "imei", &imei_);
    DBusProperties::GetString(props, kPropertyIMSI, &imsi_);
    DBusProperties::GetString(props, "esn", &esn_);
    DBusProperties::GetString(props, "mdn", &mdn_);
    DBusProperties::GetString(props, "min", &min_);
    DBusProperties::GetString(props, "firmware_revision", &firmware_revision_);

    uint32 state;
    if (DBusProperties::GetUint32(props, "state", &state))
      cellular()->set_modem_state(static_cast<Cellular::ModemState>(state));

    UpdateStatus(props);
  }
  callback.Run(error);
}

void CellularCapability::OnGetModemInfoReply(
    const ResultCallback &callback,
    const ModemHardwareInfo &info,
    const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess()) {
    manufacturer_ = info._1;
    model_id_ = info._2;
    hardware_revision_ = info._3;
    VLOG(2) << __func__ << ": " << info._1 << ", " << info._2 << ", "
            << info._3;
  }
  callback.Run(error);
}

// TODO(ers): use the supplied callback when Connect is fully asynchronous
void CellularCapability::OnConnectReply(const ResultCallback &callback,
                                        const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    cellular()->OnConnected();
  else
    cellular()->OnConnectFailed();
  if (!callback.is_null())
    callback.Run(error);
}

// TODO(ers): use the supplied callback when Disonnect is fully asynchronous
void CellularCapability::OnDisconnectReply(const ResultCallback &callback,
                                           const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    cellular()->OnDisconnected();
  else
    cellular()->OnDisconnectFailed();
  if (!callback.is_null())
    callback.Run(error);
}

void CellularCapability::OnModemStateChangedSignal(
    uint32 old_state, uint32 new_state, uint32 reason) {
  VLOG(2) << __func__ << "(" << old_state << ", " << new_state << ", "
          << reason << ")";
  // TODO(petkov): Complete this (crosbug.com/19662)
#if 0
  modem_state_ = static_cast<ModemState>(new_state);
  if (old_state == new_state) {
    return;
  }
  switch (new_state) {
    case kModemStateEnabled:
      if (old_state == kModemStateDisabled ||
          old_state == kModemStateEnabling) {
        Start();
      }
      // TODO(petkov): Handle the case when the state is downgraded to Enabled.
      break;
    default:
      break;
  }
#endif
}

}  // namespace shill
