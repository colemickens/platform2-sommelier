// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/cellular_capability_universal_cdma.h"

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular/cellular_bearer.h"
#include "shill/cellular/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/dbus_properties_proxy_interface.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/pending_activation_store.h"

using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kCellular;
static string ObjectID(CellularCapabilityUniversalCdma* c) {
  return c->cellular()->GetRpcIdentifier();
}
}

CellularCapabilityUniversalCdma::CellularCapabilityUniversalCdma(
    Cellular* cellular, ModemInfo* modem_info)
    : CellularCapabilityUniversal(cellular, modem_info),
      activation_state_(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED),
      cdma_1x_registration_state_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      cdma_evdo_registration_state_(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN),
      nid_(0),
      sid_(0),
      weak_cdma_ptr_factory_(this) {
  SLOG(this, 2) << "Cellular capability constructed: Universal CDMA";
  // TODO(armansito): Update PRL for activation over cellular.
  // See crbug.com/197330.
}

CellularCapabilityUniversalCdma::~CellularCapabilityUniversalCdma() = default;

void CellularCapabilityUniversalCdma::InitProxies() {
  SLOG(this, 2) << __func__;
  modem_cdma_proxy_ = control_interface()->CreateMM1ModemModemCdmaProxy(
      cellular()->dbus_path(), cellular()->dbus_service());
  modem_cdma_proxy_->set_activation_state_callback(
      Bind(&CellularCapabilityUniversalCdma::OnActivationStateChangedSignal,
      weak_cdma_ptr_factory_.GetWeakPtr()));
  CellularCapabilityUniversal::InitProxies();
}

void CellularCapabilityUniversalCdma::ReleaseProxies() {
  SLOG(this, 2) << __func__;
  modem_cdma_proxy_.reset();
  CellularCapabilityUniversal::ReleaseProxies();
}

void CellularCapabilityUniversalCdma::CompleteActivation(Error* error) {
  SLOG(this, 2) << __func__;
  if (cellular()->state() < Cellular::kStateEnabled) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kInvalidArguments,
                          "Unable to activate in state " +
                          Cellular::GetStateString(cellular()->state()));
    return;
  }
  ActivateAutomatic();
}

void CellularCapabilityUniversalCdma::ActivateAutomatic() {
  if (!cellular()->serving_operator_info()->IsMobileNetworkOperatorKnown() ||
      cellular()->serving_operator_info()->activation_code().empty()) {
    SLOG(this, 2) << "OTA activation cannot be run in the presence of no "
                  << "activation code.";
    return;
  }

  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierMEID, cellular()->meid());
  if (state == PendingActivationStore::kStatePending) {
    SLOG(this, 2) << "There's already a pending activation. Ignoring.";
    return;
  }
  if (state == PendingActivationStore::kStateActivated) {
    SLOG(this, 2) << "A call to OTA activation has already completed "
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
    Bind(&CellularCapabilityUniversalCdma::OnActivateReply,
         weak_cdma_ptr_factory_.GetWeakPtr(),
         ResultCallback());

  Error error;
  modem_cdma_proxy_->Activate(
      cellular()->serving_operator_info()->activation_code(),
      &error,
      activation_callback,
      kTimeoutActivate);
}

void CellularCapabilityUniversalCdma::UpdatePendingActivationState() {
  SLOG(this, 2) << __func__;
  if (IsActivated()) {
    SLOG(this, 3) << "CDMA service activated. Clear store.";
    modem_info()->pending_activation_store()->RemoveEntry(
        PendingActivationStore::kIdentifierMEID, cellular()->meid());
    return;
  }
  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierMEID, cellular()->meid());
  if (IsActivating() && state != PendingActivationStore::kStateFailureRetry) {
    SLOG(this, 3) << "OTA activation in progress. Nothing to do.";
    return;
  }
  switch (state) {
    case PendingActivationStore::kStateFailureRetry:
      SLOG(this, 3) << "OTA activation failed. Scheduling a retry.";
      cellular()->dispatcher()->PostTask(
          FROM_HERE,
          Bind(&CellularCapabilityUniversalCdma::ActivateAutomatic,
               weak_cdma_ptr_factory_.GetWeakPtr()));
      break;
    case PendingActivationStore::kStateActivated:
      SLOG(this, 3) << "OTA Activation has completed successfully. "
                    << "Waiting for activation state update to finalize.";
      break;
    default:
      break;
  }
}

bool CellularCapabilityUniversalCdma::AreProxiesInitialized() const {
  return modem_cdma_proxy_ != nullptr &&
         CellularCapabilityUniversal::AreProxiesInitialized();
}

bool CellularCapabilityUniversalCdma::IsServiceActivationRequired() const {
  // If there is no online payment portal information, it's safer to assume
  // the service does not require activation.
  if (!cellular()->serving_operator_info()->IsMobileNetworkOperatorKnown() ||
      cellular()->serving_operator_info()->olp_list().empty()) {
    return false;
  }

  // We could also use the MDN to determine whether or not the service is
  // activated, however, the CDMA ActivatonState property is a more absolute
  // and fine-grained indicator of activation status.
  return (activation_state_ == MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED);
}

bool CellularCapabilityUniversalCdma::IsActivated() const {
  return (activation_state_ == MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED);
}

void CellularCapabilityUniversalCdma::OnServiceCreated() {
  SLOG(this, 2) << __func__;
  cellular()->service()->SetActivationType(
      CellularService::kActivationTypeOTASP);
  UpdateServiceActivationStateProperty();
  HandleNewActivationStatus(MM_CDMA_ACTIVATION_ERROR_NONE);
  UpdatePendingActivationState();
}

void CellularCapabilityUniversalCdma::UpdateServiceActivationStateProperty() {
  string activation_state;
  if (IsActivating())
      activation_state = kActivationStateActivating;
  else if (IsServiceActivationRequired())
      activation_state = kActivationStateNotActivated;
  else
      activation_state = kActivationStateActivated;
  cellular()->service()->SetActivationState(activation_state);
}

void CellularCapabilityUniversalCdma::UpdateServiceOLP() {
  SLOG(this, 2) << __func__;

  // In this case, the Home Provider is trivial. All information comes from the
  // Serving Operator.
  if (!cellular()->serving_operator_info()->IsMobileNetworkOperatorKnown()) {
    return;
  }

  const vector<MobileOperatorInfo::OnlinePortal>& olp_list =
      cellular()->serving_operator_info()->olp_list();
  if (olp_list.empty()) {
    return;
  }

  if (olp_list.size() > 1) {
    SLOG(this, 1) << "Found multiple online portals. Choosing the first.";
  }
  string post_data = olp_list[0].post_data;
  base::ReplaceSubstringsAfterOffset(&post_data, 0, "${esn}",
                                     cellular()->esn());
  base::ReplaceSubstringsAfterOffset(
      &post_data, 0, "${mdn}",
      GetMdnForOLP(cellular()->serving_operator_info()));
  base::ReplaceSubstringsAfterOffset(&post_data, 0,
                                     "${meid}", cellular()->meid());
  base::ReplaceSubstringsAfterOffset(&post_data, 0, "${oem}", "GOG2");
  cellular()->service()->SetOLP(olp_list[0].url, olp_list[0].method, post_data);
}

void CellularCapabilityUniversalCdma::GetProperties() {
  SLOG(this, 2) << __func__;
  CellularCapabilityUniversal::GetProperties();

  std::unique_ptr<DBusPropertiesProxyInterface> properties_proxy =
      control_interface()->CreateDBusPropertiesProxy(
          cellular()->dbus_path(), cellular()->dbus_service());

  KeyValueStore properties(
      properties_proxy->GetAll(MM_DBUS_INTERFACE_MODEM_MODEMCDMA));
  OnModemCdmaPropertiesChanged(properties, vector<string>());
}

void CellularCapabilityUniversalCdma::OnActivationStateChangedSignal(
    uint32_t activation_state,
    uint32_t activation_error,
    const KeyValueStore& status_changes) {
  SLOG(this, 2) << __func__;

  activation_state_ =
      static_cast<MMModemCdmaActivationState>(activation_state);

  string value;
  if (status_changes.ContainsString("mdn"))
    cellular()->set_mdn(status_changes.GetString("mdn"));
  if (status_changes.ContainsString("min"))
    cellular()->set_min(status_changes.GetString("min"));
  SLOG(this, 2) << "Activation state: "
                << GetActivationStateString(activation_state_);

  HandleNewActivationStatus(activation_error);
  UpdatePendingActivationState();
}

void CellularCapabilityUniversalCdma::OnActivateReply(
    const ResultCallback& callback,
    const Error& error) {
  SLOG(this, 2) << __func__;
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

  // CellularCapabilityUniversalCdma::ActivateAutomatic passes a dummy
  // ResultCallback when it calls Activate on the proxy object, in which case
  // |callback.is_null()| will return true.
  if (!callback.is_null())
    callback.Run(error);
}

void CellularCapabilityUniversalCdma::HandleNewActivationStatus(
    uint32_t error) {
  SLOG(this, 2) << __func__ << "(" << error << ")";
  if (!cellular()->service().get()) {
    LOG(ERROR) << "In " << __func__ << "(): service is null.";
    return;
  }
  SLOG(this, 2) << "Activation State: " << activation_state_;
  cellular()->service()->SetActivationState(
      GetActivationStateString(activation_state_));
  cellular()->service()->set_error(GetActivationErrorString(error));
  UpdateServiceOLP();
}

// static
string CellularCapabilityUniversalCdma::GetActivationStateString(
    uint32_t state) {
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
string CellularCapabilityUniversalCdma::GetActivationErrorString(
    uint32_t error) {
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

void CellularCapabilityUniversalCdma::RegisterOnNetwork(
    const string& network_id,
    Error* error,
    const ResultCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

bool CellularCapabilityUniversalCdma::IsActivating() const {
  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierMEID, cellular()->meid());
  return (state == PendingActivationStore::kStatePending) ||
      (state == PendingActivationStore::kStateFailureRetry) ||
      (activation_state_ == MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING);
}

bool CellularCapabilityUniversalCdma::IsRegistered() const {
  return (cdma_1x_registration_state_ !=
              MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN ||
          cdma_evdo_registration_state_ !=
              MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
}

void CellularCapabilityUniversalCdma::SetUnregistered(bool /*searching*/) {
  cdma_1x_registration_state_ = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  cdma_evdo_registration_state_ = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
}

void CellularCapabilityUniversalCdma::SetupConnectProperties(
    KeyValueStore* properties) {
  // Skip CellularCapabilityUniversal::SetupConnectProperties() as it isn't
  // appropriate for CellularCapabilityUniversalCdma.
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

void CellularCapabilityUniversalCdma::RequirePIN(
    const string& pin, bool require,
    Error* error, const ResultCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCdma::EnterPIN(
    const string& pin,
    Error* error,
    const ResultCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCdma::UnblockPIN(
    const string& unblock_code,
    const string& pin,
    Error* error,
    const ResultCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCdma::ChangePIN(
    const string& old_pin, const string& new_pin,
    Error* error, const ResultCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCdma::Reset(Error* error,
                                            const ResultCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCdma::Scan(
    Error* error,
    const ResultStringmapsCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversalCdma::OnSimPathChanged(
    const string& sim_path) {
  // TODO(armansito): Remove once 3GPP is implemented in its own class.
}

string CellularCapabilityUniversalCdma::GetRoamingStateString() const {
  uint32_t state = cdma_evdo_registration_state_;
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

void CellularCapabilityUniversalCdma::OnPropertiesChanged(
    const string& interface,
    const KeyValueStore& changed_properties,
    const vector<string>& invalidated_properties) {
  SLOG(this, 2) << __func__ << "(" << interface << ")";
  if (interface == MM_DBUS_INTERFACE_MODEM_MODEMCDMA) {
    OnModemCdmaPropertiesChanged(changed_properties, invalidated_properties);
  } else {
    CellularCapabilityUniversal::OnPropertiesChanged(
        interface, changed_properties, invalidated_properties);
  }
}

void CellularCapabilityUniversalCdma::OnModemCdmaPropertiesChanged(
    const KeyValueStore& properties,
    const std::vector<std::string>& /*invalidated_properties*/) {
  SLOG(this, 2) << __func__;
  string str_value;
  if (properties.ContainsString(MM_MODEM_MODEMCDMA_PROPERTY_MEID)) {
    cellular()->set_meid(
        properties.GetString(MM_MODEM_MODEMCDMA_PROPERTY_MEID));
  }
  if (properties.ContainsString(MM_MODEM_MODEMCDMA_PROPERTY_ESN)) {
    cellular()->set_esn(properties.GetString(MM_MODEM_MODEMCDMA_PROPERTY_ESN));
  }

  uint32_t sid = sid_;
  uint32_t nid = nid_;
  MMModemCdmaRegistrationState state_1x = cdma_1x_registration_state_;
  MMModemCdmaRegistrationState state_evdo = cdma_evdo_registration_state_;
  bool registration_changed = false;
  if (properties.ContainsUint(
      MM_MODEM_MODEMCDMA_PROPERTY_CDMA1XREGISTRATIONSTATE)) {
    state_1x = static_cast<MMModemCdmaRegistrationState>(
        properties.GetUint(
            MM_MODEM_MODEMCDMA_PROPERTY_CDMA1XREGISTRATIONSTATE));
    registration_changed = true;
  }
  if (properties.ContainsUint(
      MM_MODEM_MODEMCDMA_PROPERTY_EVDOREGISTRATIONSTATE)) {
    state_evdo = static_cast<MMModemCdmaRegistrationState>(
        properties.GetUint(MM_MODEM_MODEMCDMA_PROPERTY_EVDOREGISTRATIONSTATE));
    registration_changed = true;
  }
  if (properties.ContainsUint(MM_MODEM_MODEMCDMA_PROPERTY_SID)) {
    sid = properties.GetUint(MM_MODEM_MODEMCDMA_PROPERTY_SID);
    registration_changed = true;
  }
  if (properties.ContainsUint(MM_MODEM_MODEMCDMA_PROPERTY_NID)) {
    nid = properties.GetUint(MM_MODEM_MODEMCDMA_PROPERTY_NID);
    registration_changed = true;
  }
  if (properties.ContainsUint(MM_MODEM_MODEMCDMA_PROPERTY_ACTIVATIONSTATE)) {
    activation_state_ = static_cast<MMModemCdmaActivationState>(
        properties.GetUint(MM_MODEM_MODEMCDMA_PROPERTY_ACTIVATIONSTATE));
    HandleNewActivationStatus(MM_CDMA_ACTIVATION_ERROR_NONE);
  }
  if (registration_changed)
    OnCdmaRegistrationChanged(state_1x, state_evdo, sid, nid);
}

void CellularCapabilityUniversalCdma::OnCdmaRegistrationChanged(
      MMModemCdmaRegistrationState state_1x,
      MMModemCdmaRegistrationState state_evdo,
      uint32_t sid, uint32_t nid) {
  SLOG(this, 2) << __func__ << ": state_1x=" << state_1x
                            << ", state_evdo=" << state_evdo;
  cdma_1x_registration_state_ = state_1x;
  cdma_evdo_registration_state_ = state_evdo;
  sid_ = sid;
  nid_ = nid;
  cellular()->serving_operator_info()->UpdateSID(base::UintToString(sid));
  cellular()->serving_operator_info()->UpdateNID(base::UintToString(nid));
  cellular()->HandleNewRegistrationState();
}

}  // namespace shill
