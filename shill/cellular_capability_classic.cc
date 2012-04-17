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

CellularCapabilityClassic::CellularCapabilityClassic(
    Cellular *cellular,
    ProxyFactory *proxy_factory)
    : CellularCapability(cellular, proxy_factory),
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
  VLOG(2) << __func__;
  proxy_.reset(proxy_factory()->CreateModemProxy(
      cellular()->dbus_path(), cellular()->dbus_owner()));
  simple_proxy_.reset(proxy_factory()->CreateModemSimpleProxy(
      cellular()->dbus_path(), cellular()->dbus_owner()));
  proxy_->set_state_changed_callback(
      Bind(&CellularCapabilityClassic::OnModemStateChangedSignal,
           weak_ptr_factory_.GetWeakPtr()));
}

void CellularCapabilityClassic::ReleaseProxies() {
  VLOG(2) << __func__;
  proxy_.reset();
  simple_proxy_.reset();
}

void CellularCapabilityClassic::FinishEnable(const ResultCallback &callback) {
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

// always called from an async context
void CellularCapabilityClassic::EnableModem(const ResultCallback &callback) {
  VLOG(2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  proxy_->Enable(true, &error, callback, kTimeoutEnable);
  if (error.IsFailure())
    callback.Run(error);
}

// always called from an async context
void CellularCapabilityClassic::DisableModem(const ResultCallback &callback) {
  VLOG(2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  proxy_->Enable(false, &error, callback, kTimeoutDefault);
  if (error.IsFailure())
      callback.Run(error);
}

// always called from an async context
void CellularCapabilityClassic::GetModemStatus(const ResultCallback &callback) {
  VLOG(2) << __func__;
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
  VLOG(2) << __func__;
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
  VLOG(2) << __func__;

  CellularTaskList *tasks = new CellularTaskList();
  ResultCallback cb =
      Bind(&CellularCapabilityClassic::StepCompletedCallback,
           weak_ptr_factory_.GetWeakPtr(), callback, false, tasks);
  ResultCallback cb_ignore_error =
      Bind(&CellularCapabilityClassic::StepCompletedCallback,
           weak_ptr_factory_.GetWeakPtr(), callback, true, tasks);
  tasks->push_back(Bind(&CellularCapabilityClassic::Disconnect,
                        weak_ptr_factory_.GetWeakPtr(),
                        static_cast<Error *>(NULL), cb_ignore_error));
  tasks->push_back(Bind(&CellularCapabilityClassic::DisableModem,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityClassic::FinishDisable,
                        weak_ptr_factory_.GetWeakPtr(), cb));

  RunNextStep(tasks);
}

void CellularCapabilityClassic::Connect(const DBusPropertiesMap &properties,
                                 Error *error,
                                 const ResultCallback &callback) {
  VLOG(2) << __func__;
  ResultCallback cb = Bind(&CellularCapabilityClassic::OnConnectReply,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  simple_proxy_->Connect(properties, error, cb, kTimeoutConnect);
}

void CellularCapabilityClassic::Disconnect(Error *error,
                                    const ResultCallback &callback) {
  VLOG(2) << __func__;
  ResultCallback cb = Bind(&CellularCapabilityClassic::OnDisconnectReply,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  proxy_->Disconnect(error, cb, kTimeoutDefault);
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

void CellularCapabilityClassic::OnGetModemStatusReply(
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

void CellularCapabilityClassic::OnGetModemInfoReply(
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
void CellularCapabilityClassic::OnConnectReply(const ResultCallback &callback,
                                               const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    cellular()->OnConnected();
  else
    cellular()->OnConnectFailed(error);
  if (!callback.is_null())
    callback.Run(error);
}

// TODO(ers): use the supplied callback when Disonnect is fully asynchronous
void CellularCapabilityClassic::OnDisconnectReply(
    const ResultCallback &callback,
    const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    cellular()->OnDisconnected();
  else
    cellular()->OnDisconnectFailed();
  if (!callback.is_null())
    callback.Run(error);
}

void CellularCapabilityClassic::OnModemStateChangedSignal(
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
