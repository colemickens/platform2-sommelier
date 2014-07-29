// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_classic.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/modem_gobi_proxy_interface.h"
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
const char CellularCapabilityClassic::kModemPropertyEnabled[] = "Enabled";
const int CellularCapabilityClassic::kTimeoutSetCarrierMilliseconds = 120000;

static Cellular::ModemState ConvertClassicToModemState(uint32_t classic_state) {
  ModemClassicState cstate = static_cast<ModemClassicState>(classic_state);
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
  }
}

CellularCapabilityClassic::CellularCapabilityClassic(
    Cellular *cellular,
    ProxyFactory *proxy_factory,
    ModemInfo *modem_info)
    : CellularCapability(cellular, proxy_factory, modem_info),
      weak_ptr_factory_(this) {
  // This class is currently instantiated only for Gobi modems so setup the
  // supported carriers list appropriately and expose it over RPC.
  cellular->set_supported_carriers({kCarrierGenericUMTS,
                                    kCarrierSprint,
                                    kCarrierVerizon});
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
  gobi_proxy_.reset();
}

bool CellularCapabilityClassic::AreProxiesInitialized() const {
  return (proxy_.get() && simple_proxy_.get() && gobi_proxy_.get());
}

void CellularCapabilityClassic::FinishEnable(const ResultCallback &callback) {
  // Normally, running the callback is the last thing done in a method.
  // In this case, we do it first, because we want to make sure that
  // the device is marked as Enabled before the registration state is
  // handled. See comment in Cellular::HandleNewRegistrationState.
  callback.Run(Error());
  GetRegistrationState();
  GetSignalQuality();
  // We expect the modem to start scanning after it has been enabled.
  // Change this if this behavior is no longer the case in the future.
  modem_info()->metrics()->NotifyDeviceEnableFinished(
      cellular()->interface_index());
  modem_info()->metrics()->NotifyDeviceScanStarted(
      cellular()->interface_index());
}

void CellularCapabilityClassic::FinishDisable(const ResultCallback &callback) {
  modem_info()->metrics()->NotifyDeviceDisableFinished(
      cellular()->interface_index());
  ReleaseProxies();
  callback.Run(Error());
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
  modem_info()->metrics()->NotifyDeviceEnableStarted(
      cellular()->interface_index());
  proxy_->Enable(true, &error, callback, kTimeoutEnable);
  if (error.IsFailure())
    callback.Run(error);
}

// always called from an async context
void CellularCapabilityClassic::DisableModem(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  modem_info()->metrics()->NotifyDeviceDisableStarted(
      cellular()->interface_index());
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
    proxy_->Disconnect(error, callback, kTimeoutDisconnect);
  else
    LOG(ERROR) << "No proxy found in disconnect.";
}

void CellularCapabilityClassic::SetCarrier(const string &carrier,
                                           Error *error,
                                           const ResultCallback &callback) {
  LOG(INFO) << __func__ << "(" << carrier << ")";
  if (!gobi_proxy_.get()) {
    gobi_proxy_.reset(proxy_factory()->CreateModemGobiProxy(
        cellular()->dbus_path(), cellular()->dbus_owner()));
  }
  CHECK(error);
  gobi_proxy_->SetCarrier(carrier, error, callback,
                          kTimeoutSetCarrierMilliseconds);
}

void CellularCapabilityClassic::OnDBusPropertiesChanged(
    const std::string &interface,
    const DBusPropertiesMap &changed_properties,
    const std::vector<std::string> &invalidated_properties) {
  SLOG(Cellular, 2) << __func__;
  bool enabled;
  // This solves a bootstrapping problem: If the modem is not yet
  // enabled, there are no proxy objects associated with the capability
  // object, so modem signals like StateChanged aren't seen. By monitoring
  // changes to the Enabled property via the ModemManager, we're able to
  // get the initialization process started, which will result in the
  // creation of the proxy objects.
  //
  // We handle all state changes to ENABLED from a disabled state (including,
  // UNKNOWN) through Cellular::OnModemStateChanged. This will try to enable
  // the device regardless of whether it has been registered with the Manager.
  //
  // All other state changes are handled from OnModemStateChangedSignal.
  if (DBusProperties::GetBool(changed_properties,
                              kModemPropertyEnabled, &enabled)) {
    SLOG(Cellular, 2) << "Property \"Enabled\" changed: " << enabled;
    Cellular::ModemState prev_modem_state = cellular()->modem_state();
    if (!Cellular::IsEnabledModemState(prev_modem_state)) {
      cellular()->OnModemStateChanged(
          enabled ? Cellular::kModemStateEnabled :
                    Cellular::kModemStateDisabled);
    }
  }
}

void CellularCapabilityClassic::OnGetModemStatusReply(
    const ResultCallback &callback,
    const DBusPropertiesMap &props,
    const Error &error) {
  string prop_value;
  SLOG(Cellular, 2) << __func__ << " " << props.size() << " props. error "
                    << error;
  if (error.IsSuccess()) {
    if (DBusProperties::GetString(props, "carrier", &prop_value)) {
      cellular()->set_carrier(prop_value);
      cellular()->home_provider_info()->UpdateOperatorName(prop_value);
    }
    if (DBusProperties::GetString(props, "meid", &prop_value)) {
      cellular()->set_meid(prop_value);
    }
    if (DBusProperties::GetString(props, "imei", &prop_value)) {
     cellular()->set_imei(prop_value);
    }
    if (DBusProperties::GetString(props, kModemPropertyIMSI, &prop_value)) {
      cellular()->set_imsi(prop_value);
      cellular()->home_provider_info()->UpdateIMSI(prop_value);
      // We do not currently obtain the IMSI OTA at all. Provide the IMSI from
      // the SIM to the serving operator as well to aid in MVNO identification.
      cellular()->serving_operator_info()->UpdateIMSI(prop_value);
    }
    if (DBusProperties::GetString(props, "esn", &prop_value)) {
      cellular()->set_esn(prop_value);
    }
    if (DBusProperties::GetString(props, "mdn", &prop_value)) {
      cellular()->set_mdn(prop_value);
    }
    if (DBusProperties::GetString(props, "min", &prop_value)) {
      cellular()->set_min(prop_value);
    }
    if (DBusProperties::GetString(props, "firmware_revision", &prop_value)) {
      cellular()->set_firmware_revision(prop_value);
    }
    UpdateStatus(props);
  }
  callback.Run(error);
}

void CellularCapabilityClassic::UpdateStatus(
    const DBusPropertiesMap &properties) {
  SLOG(Cellular, 3) << __func__;
}

void CellularCapabilityClassic::OnGetModemInfoReply(
    const ResultCallback &callback,
    const ModemHardwareInfo &info,
    const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  if (error.IsSuccess()) {
    cellular()->set_manufacturer(info._1);
    cellular()->set_model_id(info._2);
    cellular()->set_hardware_revision(info._3);
    SLOG(Cellular, 2) << __func__ << ": " << info._1 << ", " << info._2 << ", "
                      << info._3;
  }
  callback.Run(error);
}

void CellularCapabilityClassic::OnModemStateChangedSignal(
    uint32_t old_state, uint32_t new_state, uint32_t reason) {
  SLOG(Cellular, 2) << __func__ << "(" << old_state << ", " << new_state << ", "
                    << reason << ")";
  cellular()->OnModemStateChanged(ConvertClassicToModemState(new_state));
}

}  // namespace shill
