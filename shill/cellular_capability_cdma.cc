// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_cdma.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/proxy_factory.h"

using std::string;

namespace shill {

CellularCapabilityCDMA::CellularCapabilityCDMA(Cellular *cellular)
    : CellularCapability(cellular),
      task_factory_(this),
      registration_state_evdo_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      registration_state_1x_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {}

void CellularCapabilityCDMA::InitProxies() {
  VLOG(2) << __func__;
  proxy_.reset(proxy_factory()->CreateModemCDMAProxy(
      this, cellular()->dbus_path(), cellular()->dbus_owner()));
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
    cellular()->set_cdma_activation_state(
        MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING);
  }
  cellular()->HandleNewCDMAActivationState(status);
}

void CellularCapabilityCDMA::GetIdentifiers() {
  VLOG(2) << __func__;
  if (cellular()->meid().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_meid(proxy_->MEID());
    VLOG(2) << "MEID: " << cellular()->imei();
  }
}

void CellularCapabilityCDMA::GetProperties() {
  VLOG(2) << __func__;
  // No properties.
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
  string payment_url;
  if (DBusProperties::GetString(status_changes, "payment_url", &payment_url)) {
    cellular()->set_cdma_payment_url(payment_url);
    if (cellular()->service().get()) {
      cellular()->service()->set_payment_url(payment_url);
    }
  }
  cellular()->set_cdma_activation_state(activation_state);
  cellular()->HandleNewCDMAActivationState(activation_error);
}

void CellularCapabilityCDMA::OnCDMARegistrationStateChanged(
    uint32 state_1x, uint32 state_evdo) {
  VLOG(2) << __func__;
  registration_state_1x_ = state_1x;
  registration_state_evdo_ = state_evdo;
  cellular()->HandleNewRegistrationState();
}

void CellularCapabilityCDMA::OnCDMASignalQualityChanged(uint32 strength) {
  cellular()->HandleNewSignalQuality(strength);
}

}  // namespace shill
