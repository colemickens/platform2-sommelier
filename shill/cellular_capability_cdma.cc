// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_cdma.h"

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/proxy_factory.h"

using std::string;

namespace shill {

// static
unsigned int CellularCapabilityCDMA::friendly_service_name_id_ = 0;

const char CellularCapabilityCDMA::kPhoneNumber[] = "#777";

CellularCapabilityCDMA::CellularCapabilityCDMA(Cellular *cellular)
    : CellularCapability(cellular),
      task_factory_(this),
      activation_state_(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED),
      registration_state_evdo_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      registration_state_1x_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      prl_version_(0) {
  VLOG(2) << "Cellular capability constructed: CDMA";
  PropertyStore *store = cellular->mutable_store();
  store->RegisterConstUint16(flimflam::kPRLVersionProperty, &prl_version_);
}

void CellularCapabilityCDMA::OnDeviceStarted() {
  VLOG(2) << __func__;
  proxy_.reset(proxy_factory()->CreateModemCDMAProxy(
      this, cellular()->dbus_path(), cellular()->dbus_owner()));
}

void CellularCapabilityCDMA::OnDeviceStopped() {
  VLOG(2) << __func__;
  proxy_.reset();
}

void CellularCapabilityCDMA::OnServiceCreated() {
  VLOG(2) << __func__;
  cellular()->service()->set_payment_url(payment_url_);
  cellular()->service()->set_usage_url(usage_url_);
  UpdateServingOperator();
  HandleNewActivationState(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR);
}

void CellularCapabilityCDMA::UpdateStatus(const DBusPropertiesMap &properties) {
  string carrier;
  if (DBusProperties::GetString(properties, "carrier", &carrier)) {
    Cellular::Operator oper;
    oper.SetName(carrier);
    oper.SetCountry("us");
    cellular()->set_home_provider(oper);
  }
  DBusProperties::GetUint32(
      properties, "activation_state", &activation_state_);
  DBusProperties::GetUint16(properties, "prl_version", &prl_version_);
  // TODO(petkov): For now, get the payment and usage URLs from ModemManager to
  // match flimflam. In the future, get these from an alternative source (e.g.,
  // database, carrier-specific properties, etc.).
  DBusProperties::GetString(properties, "payment_url", &payment_url_);
  DBusProperties::GetString(properties, "usage_url", &usage_url_);
}

void CellularCapabilityCDMA::SetupConnectProperties(
    DBusPropertiesMap *properties) {
  (*properties)[Cellular::kConnectPropertyPhoneNumber].writer().append_string(
      kPhoneNumber);
}

void CellularCapabilityCDMA::Activate(const string &carrier, Error *error) {
  VLOG(2) << __func__ << "(" << carrier << ")";
  if (cellular()->state() != Cellular::kStateEnabled &&
      cellular()->state() != Cellular::kStateRegistered) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Unable to activate in " +
                          Cellular::GetStateString(cellular()->state()));
    return;
  }
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityCDMA::ActivateTask, carrier));
}

void CellularCapabilityCDMA::ActivateTask(const string &carrier) {
  VLOG(2) << __func__ << "(" << carrier << ")";
  if (cellular()->state() != Cellular::kStateEnabled &&
      cellular()->state() != Cellular::kStateRegistered) {
    LOG(ERROR) << "Unable to activate in "
               << Cellular::GetStateString(cellular()->state());
    return;
  }
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 status = proxy_->Activate(carrier);
  if (status == MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR) {
    activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  }
  HandleNewActivationState(status);
}

void CellularCapabilityCDMA::HandleNewActivationState(uint32 error) {
  if (!cellular()->service().get()) {
    return;
  }
  cellular()->service()->set_activation_state(
      GetActivationStateString(activation_state_));
  cellular()->service()->set_error(GetActivationErrorString(error));
}

// static
string CellularCapabilityCDMA::GetActivationStateString(uint32 state) {
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
string CellularCapabilityCDMA::GetActivationErrorString(uint32 error) {
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

void CellularCapabilityCDMA::GetIdentifiers() {
  VLOG(2) << __func__;
  if (cellular()->meid().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_meid(proxy_->MEID());
    VLOG(2) << "MEID: " << cellular()->meid();
  }
}

void CellularCapabilityCDMA::GetProperties() {
  VLOG(2) << __func__;
  // No properties.
}

bool CellularCapabilityCDMA::IsRegistered() {
  return registration_state_evdo_ != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN ||
      registration_state_1x_ != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
}

string CellularCapabilityCDMA::GetNetworkTechnologyString() const {
  if (registration_state_evdo_ != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    return flimflam::kNetworkTechnologyEvdo;
  }
  if (registration_state_1x_ != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    return flimflam::kNetworkTechnology1Xrtt;
  }
  return "";
}

string CellularCapabilityCDMA::GetRoamingStateString() const {
  uint32 state = registration_state_evdo_;
  if (state == MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    state = registration_state_1x_;
  }
  switch (state) {
    case MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN:
    case MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED:
      break;
    case MM_MODEM_CDMA_REGISTRATION_STATE_HOME:
      return flimflam::kRoamingStateHome;
    case MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING:
      return flimflam::kRoamingStateRoaming;
    default:
      NOTREACHED();
  }
  return flimflam::kRoamingStateUnknown;
}

void CellularCapabilityCDMA::GetSignalQuality() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 strength = proxy_->GetSignalQuality();
  cellular()->HandleNewSignalQuality(strength);
}

void CellularCapabilityCDMA::GetRegistrationState() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  proxy_->GetRegistrationState(
      &registration_state_1x_, &registration_state_evdo_);
  VLOG(2) << "CDMA Registration: 1x(" << registration_state_1x_
          << ") EVDO(" << registration_state_evdo_ << ")";
  cellular()->HandleNewRegistrationState();
}

string CellularCapabilityCDMA::CreateFriendlyServiceName() {
  VLOG(2) << __func__;
  if (!cellular()->carrier().empty()) {
    return cellular()->carrier();
  }
  return base::StringPrintf("CDMANetwork%u", friendly_service_name_id_++);
}

void CellularCapabilityCDMA::UpdateServingOperator() {
  VLOG(2) << __func__;
  if (cellular()->service().get()) {
    cellular()->service()->set_serving_operator(cellular()->home_provider());
  }
}

void CellularCapabilityCDMA::OnCDMAActivationStateChanged(
    uint32 activation_state,
    uint32 activation_error,
    const DBusPropertiesMap &status_changes) {
  VLOG(2) << __func__;
  string mdn;
  if (DBusProperties::GetString(status_changes, "mdn", &mdn)) {
    cellular()->set_mdn(mdn);
  }
  string min;
  if (DBusProperties::GetString(status_changes, "min", &min)) {
    cellular()->set_min(min);
  }
  if (DBusProperties::GetString(status_changes, "payment_url", &payment_url_) &&
      cellular()->service().get()) {
    cellular()->service()->set_payment_url(payment_url_);
  }
  activation_state_ = activation_state;
  HandleNewActivationState(activation_error);
}

void CellularCapabilityCDMA::OnCDMARegistrationStateChanged(
    uint32 state_1x, uint32 state_evdo) {
  VLOG(2) << __func__;
  registration_state_1x_ = state_1x;
  registration_state_evdo_ = state_evdo;
  cellular()->HandleNewRegistrationState();
}

void CellularCapabilityCDMA::OnModemManagerPropertiesChanged(
    const DBusPropertiesMap &/*properties*/) {}

void CellularCapabilityCDMA::OnCDMASignalQualityChanged(uint32 strength) {
  cellular()->HandleNewSignalQuality(strength);
}

}  // namespace shill
