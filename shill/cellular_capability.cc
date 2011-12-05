// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular.h"
#include "shill/error.h"
#include "shill/proxy_factory.h"

using base::Callback;
using std::string;

namespace shill {

const char CellularCapability::kConnectPropertyPhoneNumber[] = "number";
const char CellularCapability::kPropertyIMSI[] = "imsi";
const int CellularCapability::kTimeoutActivate = 120000;  // ms
const int CellularCapability::kTimeoutConnect = 45000;  // ms
const int CellularCapability::kTimeoutDefault = 5000;  // ms
const int CellularCapability::kTimeoutRegister = 90000;  // ms
const int CellularCapability::kTimeoutScan = 120000;  // ms

CellularCapability::MultiStepAsyncCallHandler::MultiStepAsyncCallHandler(
    EventDispatcher *dispatcher)
    : AsyncCallHandler(),
      dispatcher_(dispatcher) {
}

CellularCapability::MultiStepAsyncCallHandler::~MultiStepAsyncCallHandler() {
}

void CellularCapability::MultiStepAsyncCallHandler::AddTask(Task *task) {
  tasks_.push_back(task);
}

bool CellularCapability::MultiStepAsyncCallHandler::CompleteOperation() {
  if (tasks_.empty()) {
    DoReturn();
    return true;
  } else {
    PostNextTask();
  }
  return false;
}

void CellularCapability::MultiStepAsyncCallHandler::PostNextTask() {
  VLOG(2) << __func__ << ": " << tasks_.size() << " remaining actions";
  if (tasks_.empty())
    return;
  Task *task = tasks_[0];
  tasks_.weak_erase(tasks_.begin());
  dispatcher_->PostTask(task);
}

CellularCapability::CellularCapability(Cellular *cellular,
                                       ProxyFactory *proxy_factory)
    : allow_roaming_(false),
      cellular_(cellular),
      proxy_factory_(proxy_factory) {
  PropertyStore *store = cellular->mutable_store();
  store->RegisterConstString(flimflam::kCarrierProperty, &carrier_);
  store->RegisterBool(flimflam::kCellularAllowRoamingProperty, &allow_roaming_);
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

void CellularCapability::StartModem() {
  VLOG(2) << __func__;
  proxy_.reset(proxy_factory()->CreateModemProxy(
      this, cellular()->dbus_path(), cellular()->dbus_owner()));
  simple_proxy_.reset(proxy_factory()->CreateModemSimpleProxy(
      this, cellular()->dbus_path(), cellular()->dbus_owner()));
}

void CellularCapability::StopModem() {
  VLOG(2) << __func__;
  proxy_.reset();
  simple_proxy_.reset();
}

void CellularCapability::CompleteOperation(AsyncCallHandler *reply_handler) {
  if (reply_handler && reply_handler->Complete())
      delete reply_handler;
}

void CellularCapability::CompleteOperation(AsyncCallHandler *reply_handler,
                                           const Error &error) {
  if (reply_handler && reply_handler->Complete(error))
    delete reply_handler;
}

void CellularCapability::OnUnsupportedOperation(
    const char *operation,
    AsyncCallHandler *call_handler) {
  Error error;
  string message("The ");
  message.append(operation).append(" operation is not supported.");
  Error::PopulateAndLog(&error, Error::kNotSupported, message);
  CompleteOperation(call_handler, error);
}

void CellularCapability::EnableModem(AsyncCallHandler *call_handler) {
  VLOG(2) << __func__;
  proxy_->Enable(true, call_handler, kTimeoutDefault);
}

// TODO(ers): convert to async once we have a way for the callback
// to distinguish an enable from a disable, and once the Stop() operation
// has been converted to multi-step async.
void CellularCapability::DisableModem(AsyncCallHandler * /*call_handler*/) {
  try {
    proxy_->Enable(false);
    cellular()->OnModemDisabled();
  } catch (const DBus::Error e) {
    LOG(WARNING) << "Disable failed: " << e.what();
  }
}

void CellularCapability::GetModemStatus(AsyncCallHandler *call_handler) {
  VLOG(2) << __func__;
  simple_proxy_->GetModemStatus(call_handler, kTimeoutDefault);
}

void CellularCapability::GetModemInfo(AsyncCallHandler *call_handler) {
  VLOG(2) << __func__;
  proxy_->GetModemInfo(call_handler, kTimeoutDefault);
}

// TODO(ers): make this async (supply an AsyncCallHandler arg)
void CellularCapability::Connect(const DBusPropertiesMap &properties) {
  VLOG(2) << __func__;
  simple_proxy_->Connect(properties, NULL, kTimeoutConnect);
}

// TODO(ers): convert to async once the Stop() operation
// has been converted to multi-step async.
void CellularCapability::Disconnect() {
  VLOG(2) << __func__;
  try {
    proxy_->Disconnect();
    cellular()->OnDisconnected();
  } catch (const DBus::Error e) {
    LOG(WARNING) << "Disconnect failed: " << e.what();
  }
}

void CellularCapability::Activate(const string &/*carrier*/,
                                  AsyncCallHandler *call_handler) {
  OnUnsupportedOperation(__func__, call_handler);
}

void CellularCapability::RegisterOnNetwork(
    const string &/*network_id*/, AsyncCallHandler *call_handler) {
  OnUnsupportedOperation(__func__, call_handler);
}

void CellularCapability::RequirePIN(
    const string &/*pin*/, bool /*require*/, AsyncCallHandler *call_handler) {
  OnUnsupportedOperation(__func__, call_handler);
}

void CellularCapability::EnterPIN(const string &/*pin*/,
                                  AsyncCallHandler *call_handler) {
  OnUnsupportedOperation(__func__, call_handler);
}

void CellularCapability::UnblockPIN(const string &/*unblock_code*/,
                                    const string &/*pin*/,
                                    AsyncCallHandler *call_handler) {
  OnUnsupportedOperation(__func__, call_handler);
}

void CellularCapability::ChangePIN(const string &/*old_pin*/,
                                   const string &/*new_pin*/,
                                   AsyncCallHandler *call_handler) {
  OnUnsupportedOperation(__func__, call_handler);
}

void CellularCapability::Scan(AsyncCallHandler *call_handler) {
  OnUnsupportedOperation(__func__, call_handler);
}

void CellularCapability::OnModemEnableCallback(const Error &error,
                                               AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    cellular()->OnModemEnabled();
  CompleteOperation(call_handler, error);
}

void CellularCapability::OnGetModemStatusCallback(
    const DBusPropertiesMap &props, const Error &error,
    AsyncCallHandler *call_handler) {
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
  CompleteOperation(call_handler, error);
}

void CellularCapability::OnGetModemInfoCallback(
    const ModemHardwareInfo &info,
    const Error &error,
    AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess()) {
    manufacturer_ = info._1;
    model_id_ = info._2;
    hardware_revision_ = info._3;
    VLOG(2) << __func__ << ": " << info._1 << ", " << info._2 << ", "
            << info._3;
  }
  CompleteOperation(call_handler, error);
}

void CellularCapability::OnConnectCallback(const Error &error,
                                           AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    cellular()->OnConnected();
  else
    cellular()->OnConnectFailed();
  CompleteOperation(call_handler, error);
}

void CellularCapability::OnDisconnectCallback(const Error &error,
                                              AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    cellular()->OnDisconnected();
  else
    cellular()->OnDisconnectFailed();
  CompleteOperation(call_handler, error);
}

void CellularCapability::OnModemStateChanged(
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
