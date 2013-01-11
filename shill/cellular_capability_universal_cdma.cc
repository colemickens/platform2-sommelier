// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal_cdma.h"

#include <chromeos/dbus/service_constants.h>
#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>

#include "shill/cellular_operator_info.h"
#include "shill/dbus_properties_proxy_interface.h"
#include "shill/logging.h"
#include "shill/proxy_factory.h"

#ifdef MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN
#error "Do not include mm-modem.h"
#endif

using base::UintToString;

using std::string;
using std::vector;

namespace shill {

namespace {

string FormattedSID(const string &sid) {
  return "[SID=" + sid + "]";
}

}  // namespace

// static
unsigned int
CellularCapabilityUniversalCDMA::friendly_service_name_id_cdma_ = 0;

CellularCapabilityUniversalCDMA::CellularCapabilityUniversalCDMA(
    Cellular *cellular,
    ProxyFactory *proxy_factory,
    ModemInfo *modem_info)
    : CellularCapabilityUniversal(cellular,
                                  proxy_factory,
                                  modem_info),
      cdma_1x_registration_state_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      cdma_evdo_registration_state_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      nid_(0),
      sid_(0) {
  SLOG(Cellular, 2) << "Cellular capability constructed: Universal CDMA";
  // TODO(armansito): Need to expose PRL version here to carry through
  // activation. PRL version is not obtainable from ModemManager and will
  // need to be stored in a special place (such as CellularOperatorInfo).
  // See crbug.com/197330.
}

void CellularCapabilityUniversalCDMA::InitProxies() {
  SLOG(Cellular, 2) << __func__;
  modem_cdma_proxy_.reset(
      proxy_factory()->CreateMM1ModemModemCdmaProxy(cellular()->dbus_path(),
                                                    cellular()->dbus_owner()));
  CellularCapabilityUniversal::InitProxies();
}

void CellularCapabilityUniversalCDMA::ReleaseProxies() {
  SLOG(Cellular, 2) << __func__;
  modem_cdma_proxy_.reset();
  CellularCapabilityUniversal::ReleaseProxies();
}

void CellularCapabilityUniversalCDMA::Activate(
    const string &carrier,
    Error *error,
    const ResultCallback &callback) {
  // TODO(armansito): Implement activation.
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCDMA::CompleteActivation(Error *error) {
  // TODO(armansito): Implement activation.
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCDMA::DisconnectCleanup() {
  SLOG(Cellular, 2) << __func__;
  // TODO(armansito): Handle activation logic here.
}

void CellularCapabilityUniversalCDMA::OnServiceCreated() {
  SLOG(Cellular, 2) << __func__;
  // TODO (armansito): Set storage identifier here  based on the superclass
  // implementation.
  bool activation_required = IsServiceActivationRequired();
  cellular()->service()->SetActivationState(
      activation_required ?
      flimflam::kActivationStateNotActivated :
      flimflam::kActivationStateActivated);
  cellular()->service()->SetActivateOverNonCellularNetwork(activation_required);
  UpdateServingOperator();
  UpdateOLP();
}

void CellularCapabilityUniversalCDMA::UpdateOLP() {
  SLOG(Cellular,2) << __func__;
  if (!modem_info()->cellular_operator_info())
    return;

  const CellularService::OLP *result =
      modem_info()->cellular_operator_info()->GetOLPBySID(
          UintToString(sid_));
  if (!result)
    return;

  CellularService::OLP olp;
  olp.CopyFrom(*result);
  string post_data = olp.GetPostData();
  ReplaceSubstringsAfterOffset(&post_data, 0, "${esn}", esn());
  ReplaceSubstringsAfterOffset(&post_data, 0, "${mdn}", mdn());
  ReplaceSubstringsAfterOffset(&post_data, 0, "${meid}", meid());
  olp.SetPostData(post_data);
  cellular()->service()->SetOLP(olp);
}

void CellularCapabilityUniversalCDMA::GetProperties() {
  SLOG(Cellular, 2) << __func__;
  CellularCapabilityUniversal::GetProperties();

  scoped_ptr<DBusPropertiesProxyInterface> properties_proxy(
      proxy_factory()->CreateDBusPropertiesProxy(cellular()->dbus_path(),
                                                 cellular()->dbus_owner()));
  DBusPropertiesMap properties(
      properties_proxy->GetAll(MM_DBUS_INTERFACE_MODEM_MODEMCDMA));
  OnModemCDMAPropertiesChanged(properties, vector<string>());
}

string CellularCapabilityUniversalCDMA::CreateFriendlyServiceName() {
  SLOG(Cellular, 2) << __func__ << ": " << GetRoamingStateString();

  if (provider_.GetCode().empty()) {
    UpdateOperatorInfo();
  }

  string name = provider_.GetName();
  if (!name.empty()) {
    // TODO(armansito): We may need to show the provider name in a
    // specific way if roaming.
    return name;
  }

  string code = provider_.GetCode();
  if (!code.empty()) {
    return "cellular_sid_" + code;
  }

  return base::StringPrintf("CDMANetwork%u", friendly_service_name_id_cdma_++);
}

void CellularCapabilityUniversalCDMA::UpdateOperatorInfo() {
  SLOG(Cellular, 2) << __func__;

  if (sid_ == 0 || !modem_info()->cellular_operator_info()) {
    SLOG(Cellular, 2) << "No provider is currently available.";
    provider_.SetCode("");
    return;
  }

  string sid = UintToString(sid_);
  const CellularOperatorInfo::CellularOperator *provider =
      modem_info()->cellular_operator_info()->GetCellularOperatorBySID(sid);
  if (!provider) {
    SLOG(Cellular, 2) << "CDMA provider with "
                      << FormattedSID(sid)
                      << " not found.";
    return;
  }

  if (!provider->name_list().empty()) {
    provider_.SetName(provider->name_list()[0].name);
  }
  provider_.SetCode(sid);
  provider_.SetCountry(provider->country());

  // TODO(armansito): The CDMA interface only returns information about the
  // current serving carrier, so for now both the home provider and the
  // serving operator will be the same in case of roaming. We should figure
  // out if there is a way to (and whether or not it is necessary to)
  // determine if we're roaming.
  cellular()->set_home_provider(provider_);

  UpdateServingOperator();
}

void CellularCapabilityUniversalCDMA::UpdateServingOperator() {
  SLOG(Cellular, 2) << __func__;
  if (cellular()->service().get()) {
    cellular()->service()->SetServingOperator(cellular()->home_provider());
  }
}

void CellularCapabilityUniversalCDMA::Register(const ResultCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

void CellularCapabilityUniversalCDMA::RegisterOnNetwork(
    const string &network_id,
    Error *error,
    const ResultCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

bool CellularCapabilityUniversalCDMA::IsRegistered() {
  return (cdma_1x_registration_state_ ==
              MM_MODEM_CDMA_REGISTRATION_STATE_HOME ||
          cdma_1x_registration_state_ ==
              MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING ||
          cdma_evdo_registration_state_ ==
              MM_MODEM_CDMA_REGISTRATION_STATE_HOME ||
          cdma_evdo_registration_state_ ==
              MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
}

void CellularCapabilityUniversalCDMA::SetUnregistered(bool /*searching*/) {
  cdma_1x_registration_state_ = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  cdma_evdo_registration_state_ = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
}

void CellularCapabilityUniversalCDMA::RequirePIN(
    const string &pin, bool require,
    Error *error, const ResultCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

void CellularCapabilityUniversalCDMA::EnterPIN(
    const string &pin,
    Error *error,
    const ResultCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

void CellularCapabilityUniversalCDMA::UnblockPIN(
    const string &unblock_code,
    const string &pin,
    Error *error,
    const ResultCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

void CellularCapabilityUniversalCDMA::ChangePIN(
    const string &old_pin, const string &new_pin,
    Error *error, const ResultCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

void CellularCapabilityUniversalCDMA::Scan(Error *error,
                                           const ResultCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

void CellularCapabilityUniversalCDMA::OnSimPathChanged(
    const string &sim_path) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

string CellularCapabilityUniversalCDMA::GetRoamingStateString() const {
  uint32 state = cdma_evdo_registration_state_;
  if (state == MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    state = cdma_1x_registration_state_;
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

void CellularCapabilityUniversalCDMA::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  SLOG(Cellular, 2) << __func__ << "(" << interface << ")";
  if (interface == MM_DBUS_INTERFACE_MODEM_MODEMCDMA) {
    OnModemCDMAPropertiesChanged(changed_properties, invalidated_properties);
  } else {
    CellularCapabilityUniversal::OnDBusPropertiesChanged(
        interface, changed_properties, invalidated_properties);
  }
}

void CellularCapabilityUniversalCDMA::OnModemCDMAPropertiesChanged(
    const DBusPropertiesMap &properties,
    const std::vector<std::string> &/*invalidated_properties*/) {
  SLOG(Cellular, 2) << __func__;
  string str_value;
  if (DBusProperties::GetString(properties,
                                MM_MODEM_MODEMCDMA_PROPERTY_MEID,
                                &str_value))
    set_meid(str_value);
  if (DBusProperties::GetString(properties,
                                MM_MODEM_MODEMCDMA_PROPERTY_ESN,
                                &str_value))
    set_esn(str_value);

  uint32_t sid = sid_;
  uint32_t nid = nid_;
  MMModemCdmaRegistrationState state_1x = cdma_1x_registration_state_;
  MMModemCdmaRegistrationState state_evdo = cdma_evdo_registration_state_;
  bool registration_changed = false;
  uint32_t uint_value;
  if (DBusProperties::GetUint32(
      properties,
      MM_MODEM_MODEMCDMA_PROPERTY_CDMA1XREGISTRATIONSTATE,
      &uint_value)) {
    state_1x = static_cast<MMModemCdmaRegistrationState>(uint_value);
    registration_changed = true;
  }
  if (DBusProperties::GetUint32(
      properties,
      MM_MODEM_MODEMCDMA_PROPERTY_EVDOREGISTRATIONSTATE,
      &uint_value)) {
    state_evdo = static_cast<MMModemCdmaRegistrationState>(uint_value);
    registration_changed = true;
  }
  if (DBusProperties::GetUint32(
      properties,
      MM_MODEM_MODEMCDMA_PROPERTY_SID,
      &uint_value)) {
    sid = uint_value;
    registration_changed = true;
  }
  if (DBusProperties::GetUint32(
      properties,
      MM_MODEM_MODEMCDMA_PROPERTY_NID,
      &uint_value)) {
    nid = uint_value;
    registration_changed = true;
  }
  if (registration_changed)
    OnCDMARegistrationChanged(state_1x, state_evdo, sid, nid);
}

void CellularCapabilityUniversalCDMA::OnCDMARegistrationChanged(
      MMModemCdmaRegistrationState state_1x,
      MMModemCdmaRegistrationState state_evdo,
      uint32_t sid, uint32_t nid) {
  SLOG(Cellular, 2) << __func__
                    << ": state_1x=" << state_1x
                    << ", state_evdo=" << state_evdo;
  cdma_1x_registration_state_ = state_1x;
  cdma_evdo_registration_state_ = state_evdo;
  sid_ = sid;
  nid_ = nid;
  UpdateOperatorInfo();
  cellular()->HandleNewRegistrationState();
}

}  // namespace shill
