// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal_cdma.h"

#include <chromeos/dbus/service_constants.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include "shill/cellular_bearer.h"
#include "shill/cellular_operator_info.h"
#include "shill/dbus_properties_proxy_interface.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/pending_activation_store.h"
#include "shill/proxy_factory.h"

#ifdef MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN
#error "Do not include mm-modem.h"
#endif

using base::UintToString;

using std::string;
using std::vector;

namespace shill {

namespace {

const char kPhoneNumber[] = "#777";
const char kPropertyConnectNumber[] = "number";

string FormattedSID(const string &sid) {
  return "[SID=" + sid + "]";
}

}  // namespace

CellularCapabilityUniversalCDMA::CellularCapabilityUniversalCDMA(
    Cellular *cellular,
    ProxyFactory *proxy_factory,
    ModemInfo *modem_info)
    : CellularCapabilityUniversal(cellular, proxy_factory, modem_info),
      weak_cdma_ptr_factory_(this),
      activation_state_(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED),
      cdma_1x_registration_state_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      cdma_evdo_registration_state_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      nid_(0),
      sid_(0) {
  SLOG(Cellular, 2) << "Cellular capability constructed: Universal CDMA";
  // TODO(armansito): Update PRL for activation over cellular.
  // See crbug.com/197330.
}

CellularCapabilityUniversalCDMA::~CellularCapabilityUniversalCDMA() {}

void CellularCapabilityUniversalCDMA::InitProxies() {
  SLOG(Cellular, 2) << __func__;
  modem_cdma_proxy_.reset(
      proxy_factory()->CreateMM1ModemModemCdmaProxy(cellular()->dbus_path(),
                                                    cellular()->dbus_owner()));
  modem_cdma_proxy_->set_activation_state_callback(
      Bind(&CellularCapabilityUniversalCDMA::OnActivationStateChangedSignal,
      weak_cdma_ptr_factory_.GetWeakPtr()));
  CellularCapabilityUniversal::InitProxies();
}

void CellularCapabilityUniversalCDMA::ReleaseProxies() {
  SLOG(Cellular, 2) << __func__;
  modem_cdma_proxy_.reset();
  CellularCapabilityUniversal::ReleaseProxies();
}

void CellularCapabilityUniversalCDMA::Activate(const string &carrier,
                                               Error *error,
                                               const ResultCallback &callback) {
  // Currently activation over the cellular network is not supported using
  // ModemManager-next. Service activation is currently carried through over
  // non-cellular networks and only the final step of the OTA activation
  // procedure ("automatic activation") is performed by this class.
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCDMA::CompleteActivation(Error *error) {
  SLOG(Cellular, 2) << __func__;
  if (cellular()->state() < Cellular::kStateEnabled) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Unable to activate in state " +
                          Cellular::GetStateString(cellular()->state()));
    return;
  }
  ActivateAutomatic();
}

void CellularCapabilityUniversalCDMA::ActivateAutomatic() {
  if (activation_code_.empty()) {
    SLOG(Cellular, 2) << "OTA activation cannot be run in the presence of no "
                      << "activation code.";
    return;
  }
  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierMEID, cellular()->meid());
  if (state == PendingActivationStore::kStatePending) {
    SLOG(Cellular, 2) << "There's already a pending activation. Ignoring.";
    return;
  }
  if (state == PendingActivationStore::kStateActivated) {
    SLOG(Cellular, 2) << "A call to OTA activation has already completed "
                      << "successfully. Ignoring.";
    return;
  }

  // Mark as pending activation, so that shill can recover if anything fails
  // during OTA activation.
  modem_info()->pending_activation_store()->SetActivationState(
      PendingActivationStore::kIdentifierMEID,
      cellular()->meid(),
      PendingActivationStore::kStatePending);

  // Initiate OTA activation.
  ResultCallback activation_callback =
    Bind(&CellularCapabilityUniversalCDMA::OnActivateReply,
         weak_cdma_ptr_factory_.GetWeakPtr(),
         ResultCallback());

  Error error;
  modem_cdma_proxy_->Activate(
      activation_code_, &error, activation_callback, kTimeoutActivate);
}

void CellularCapabilityUniversalCDMA::UpdatePendingActivationState() {
  SLOG(Cellular, 2) << __func__;
  if (IsActivated()) {
    SLOG(Cellular, 3) << "CDMA service activated. Clear store.";
    modem_info()->pending_activation_store()->RemoveEntry(
        PendingActivationStore::kIdentifierMEID, cellular()->meid());
    return;
  }
  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierMEID, cellular()->meid());
  if (IsActivating() && state != PendingActivationStore::kStateFailureRetry) {
    SLOG(Cellular, 3) << "OTA activation in progress. Nothing to do.";
    return;
  }
  switch (state) {
    case PendingActivationStore::kStateFailureRetry:
      SLOG(Cellular, 3) << "OTA activation failed. Scheduling a retry.";
      cellular()->dispatcher()->PostTask(
          Bind(&CellularCapabilityUniversalCDMA::ActivateAutomatic,
               weak_cdma_ptr_factory_.GetWeakPtr()));
      break;
    case PendingActivationStore::kStateActivated:
      SLOG(Cellular, 3) << "OTA Activation has completed successfully. "
                        << "Waiting for activation state update to finalize.";
      break;
    default:
      break;
  }
}

bool CellularCapabilityUniversalCDMA::IsServiceActivationRequired() const {
  // If there is no online payment portal information, it's safer to assume
  // the service does not require activation.
  if (!modem_info()->cellular_operator_info())
    return false;

  const CellularOperatorInfo::OLP *olp =
      modem_info()->cellular_operator_info()->GetOLPBySID(UintToString(sid_));
  if (!olp)
    return false;

  // We could also use the MDN to determine whether or not the service is
  // activated, however, the CDMA ActivatonState property is a more absolute
  // and fine-grained indicator of activation status.
  return (activation_state_ == MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED);
}

bool CellularCapabilityUniversalCDMA::IsActivated() const {
  return (activation_state_ == MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED);
}

void CellularCapabilityUniversalCDMA::OnServiceCreated() {
  SLOG(Cellular, 2) << __func__;
  UpdateServiceActivationStateProperty();
  UpdateServingOperator();
  HandleNewActivationStatus(MM_CDMA_ACTIVATION_ERROR_NONE);
  UpdatePendingActivationState();
}

void CellularCapabilityUniversalCDMA::UpdateServiceActivationStateProperty() {
  bool activation_required = IsServiceActivationRequired();
  cellular()->service()->SetActivateOverNonCellularNetwork(activation_required);
  string activation_state;
  if (IsActivating())
      activation_state = kActivationStateActivating;
  else if (activation_required)
      activation_state = kActivationStateNotActivated;
  else
      activation_state = kActivationStateActivated;
  cellular()->service()->SetActivationState(activation_state);
}

void CellularCapabilityUniversalCDMA::UpdateServiceOLP() {
  SLOG(Cellular, 2) << __func__;

  // In this case, the Home Provider is trivial. All information comes from the
  // Serving Operator.
  if (!cellular()->serving_operator_info()->IsMobileNetworkOperatorKnown()) {
    return;
  }

  const vector<MobileOperatorInfo::OnlinePortal> &olp_list =
      cellular()->serving_operator_info()->olp_list();
  if (olp_list.empty()) {
    return;
  }

  if (olp_list.size() > 1) {
    SLOG(Cellular, 1) << "Found multiple online portals. Choosing the first.";
  }
  string post_data = olp_list[0].post_data;
  ReplaceSubstringsAfterOffset(&post_data, 0, "${esn}", cellular()->esn());
  ReplaceSubstringsAfterOffset(
      &post_data, 0, "${mdn}",
      GetMdnForOLP(cellular()->serving_operator_info()));
  ReplaceSubstringsAfterOffset(&post_data, 0, "${meid}", cellular()->meid());
  ReplaceSubstringsAfterOffset(&post_data, 0, "${oem}", "GOG2");
  cellular()->service()->SetOLP(olp_list[0].url, olp_list[0].method, post_data);
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
    // If a matching provider is not found, shill should update the
    // Cellular.ServingOperator property to to display the Sid.
    provider_.SetCode(sid);
    provider_.SetCountry("");
    provider_.SetName("");
    activation_code_.clear();
  } else {
    if (!provider->name_list().empty()) {
      provider_.SetName(provider->name_list()[0].name);
    }
    provider_.SetCode(sid);
    provider_.SetCountry(provider->country());

    activation_code_ = provider->activation_code();
  }

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

void CellularCapabilityUniversalCDMA::OnActivationStateChangedSignal(
    uint32 activation_state,
    uint32 activation_error,
    const DBusPropertiesMap &status_changes) {
  SLOG(Cellular, 2) << __func__;

  activation_state_ =
      static_cast<MMModemCdmaActivationState>(activation_state);

  string value;
  if (DBusProperties::GetString(status_changes, "mdn", &value))
    cellular()->set_mdn(value);
  if (DBusProperties::GetString(status_changes, "min", &value))
    cellular()->set_min(value);

  SLOG(Cellular, 2) << "Activation state: "
                    << GetActivationStateString(activation_state_);

  HandleNewActivationStatus(activation_error);
  UpdatePendingActivationState();
}

void CellularCapabilityUniversalCDMA::OnActivateReply(
    const ResultCallback &callback,
    const Error &error) {
  SLOG(Cellular, 2) << __func__;
  if (error.IsSuccess()) {
    LOG(INFO) << "Activation completed successfully.";
    modem_info()->pending_activation_store()->SetActivationState(
        PendingActivationStore::kIdentifierMEID,
        cellular()->meid(),
        PendingActivationStore::kStateActivated);
  } else {
    LOG(ERROR) << "Activation failed with error: " << error;
    modem_info()->pending_activation_store()->SetActivationState(
        PendingActivationStore::kIdentifierMEID,
        cellular()->meid(),
        PendingActivationStore::kStateFailureRetry);
  }
  UpdatePendingActivationState();

  // CellularCapabilityUniversalCDMA::ActivateAutomatic passes a dummy
  // ResultCallback when it calls Activate on the proxy object, in which case
  // |callback.is_null()| will return true.
  if (!callback.is_null())
    callback.Run(error);
}

void CellularCapabilityUniversalCDMA::HandleNewActivationStatus(uint32 error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  if (!cellular()->service().get()) {
    LOG(ERROR) << "In " << __func__ << "(): service is null.";
    return;
  }
  SLOG(Cellular, 2) << "Activation State: " << activation_state_;
  cellular()->service()->SetActivationState(
      GetActivationStateString(activation_state_));
  cellular()->service()->set_error(GetActivationErrorString(error));
  UpdateServiceOLP();
}

// static
string CellularCapabilityUniversalCDMA::GetActivationStateString(
    uint32 state) {
  switch (state) {
    case MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED:
      return kActivationStateActivated;
    case MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING:
      return kActivationStateActivating;
    case MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED:
      return kActivationStateNotActivated;
    case MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED:
      return kActivationStatePartiallyActivated;
    default:
      return kActivationStateUnknown;
  }
}

// static
string CellularCapabilityUniversalCDMA::GetActivationErrorString(
    uint32 error) {
  switch (error) {
    case MM_CDMA_ACTIVATION_ERROR_WRONG_RADIO_INTERFACE:
      return kErrorNeedEvdo;
    case MM_CDMA_ACTIVATION_ERROR_ROAMING:
      return kErrorNeedHomeNetwork;
    case MM_CDMA_ACTIVATION_ERROR_COULD_NOT_CONNECT:
    case MM_CDMA_ACTIVATION_ERROR_SECURITY_AUTHENTICATION_FAILED:
    case MM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED:
      return kErrorOtaspFailed;
    case MM_CDMA_ACTIVATION_ERROR_NONE:
      return "";
    case MM_CDMA_ACTIVATION_ERROR_NO_SIGNAL:
    default:
      return kErrorActivationFailed;
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

bool CellularCapabilityUniversalCDMA::IsActivating() const {
  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierMEID, cellular()->meid());
  return (state == PendingActivationStore::kStatePending) ||
      (state == PendingActivationStore::kStateFailureRetry) ||
      (activation_state_ == MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING);
}

bool CellularCapabilityUniversalCDMA::IsRegistered() const {
  return (cdma_1x_registration_state_ !=
              MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN ||
          cdma_evdo_registration_state_ !=
              MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
}

void CellularCapabilityUniversalCDMA::SetUnregistered(bool /*searching*/) {
  cdma_1x_registration_state_ = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  cdma_evdo_registration_state_ = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
}

void CellularCapabilityUniversalCDMA::SetupConnectProperties(
    DBusPropertiesMap *properties) {
  (*properties)[kPropertyConnectNumber].writer().append_string(
      kPhoneNumber);
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

void CellularCapabilityUniversalCDMA::Scan(
    Error *error,
    const ResultStringmapsCallback &callback) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
  OnUnsupportedOperation(__func__, error);
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
      return kRoamingStateHome;
    case MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING:
      return kRoamingStateRoaming;
    default:
      NOTREACHED();
  }
  return kRoamingStateUnknown;
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
    cellular()->set_meid(str_value);
  if (DBusProperties::GetString(properties,
                                MM_MODEM_MODEMCDMA_PROPERTY_ESN,
                                &str_value))
    cellular()->set_esn(str_value);

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
  if (DBusProperties::GetUint32(
      properties,
      MM_MODEM_MODEMCDMA_PROPERTY_ACTIVATIONSTATE,
      &uint_value)) {
    activation_state_ = static_cast<MMModemCdmaActivationState>(uint_value);
    HandleNewActivationStatus(MM_CDMA_ACTIVATION_ERROR_NONE);
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
  cellular()->serving_operator_info()->UpdateSID(UintToString(sid));
  cellular()->serving_operator_info()->UpdateNID(UintToString(nid));
  UpdateOperatorInfo();
  cellular()->HandleNewRegistrationState();
}

}  // namespace shill
