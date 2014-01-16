// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal.h"

#include <base/bind.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <mobile_provider.h>
#include <ModemManager/ModemManager.h>

#include <string>
#include <vector>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular_service.h"
#include "shill/dbus_properties_proxy_interface.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/pending_activation_store.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"

#ifdef MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN
#error "Do not include mm-modem.h"
#endif

using base::Bind;
using base::Closure;
using std::string;
using std::vector;

namespace shill {

// static
const char CellularCapabilityUniversal::kConnectPin[] = "pin";
const char CellularCapabilityUniversal::kConnectOperatorId[] = "operator-id";
const char CellularCapabilityUniversal::kConnectApn[] = "apn";
const char CellularCapabilityUniversal::kConnectIPType[] = "ip-type";
const char CellularCapabilityUniversal::kConnectUser[] = "user";
const char CellularCapabilityUniversal::kConnectPassword[] = "password";
const char CellularCapabilityUniversal::kConnectNumber[] = "number";
const char CellularCapabilityUniversal::kConnectAllowRoaming[] =
    "allow-roaming";
const char CellularCapabilityUniversal::kConnectRMProtocol[] = "rm-protocol";
const int64
CellularCapabilityUniversal::kActivationRegistrationTimeoutMilliseconds =
    20000;
const int64 CellularCapabilityUniversal::kEnterPinTimeoutMilliseconds = 20000;
const int64
CellularCapabilityUniversal::kRegistrationDroppedUpdateTimeoutMilliseconds =
    15000;
const char CellularCapabilityUniversal::kGenericServiceNamePrefix[] =
    "Mobile Network";
const char CellularCapabilityUniversal::kRootPath[] = "/";
const char CellularCapabilityUniversal::kStatusProperty[] = "status";
const char CellularCapabilityUniversal::kOperatorLongProperty[] =
    "operator-long";
const char CellularCapabilityUniversal::kOperatorShortProperty[] =
    "operator-short";
const char CellularCapabilityUniversal::kOperatorCodeProperty[] =
    "operator-code";
const char CellularCapabilityUniversal::kOperatorAccessTechnologyProperty[] =
    "access-technology";
const char CellularCapabilityUniversal::kIpConfigPropertyMethod[] = "method";
const char CellularCapabilityUniversal::kALT3100ModelId[] = "ALT3100";
const char CellularCapabilityUniversal::kE362ModelId[] = "E362 WWAN";
const int CellularCapabilityUniversal::kSetPowerStateTimeoutMilliseconds =
    20000;
unsigned int CellularCapabilityUniversal::friendly_service_name_id_ = 0;

namespace {

const char kPhoneNumber[] = "*99#";

// This identifier is specified in the cellular_operator_info file.
const char kVzwIdentifier[] = "vzw";
const size_t kVzwMdnLength = 10;

string AccessTechnologyToString(uint32 access_technologies) {
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_LTE)
    return kNetworkTechnologyLte;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_EVDO0 |
                              MM_MODEM_ACCESS_TECHNOLOGY_EVDOA |
                              MM_MODEM_ACCESS_TECHNOLOGY_EVDOB))
    return kNetworkTechnologyEvdo;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_1XRTT)
    return kNetworkTechnology1Xrtt;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS)
    return kNetworkTechnologyHspaPlus;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_HSPA |
                              MM_MODEM_ACCESS_TECHNOLOGY_HSUPA |
                              MM_MODEM_ACCESS_TECHNOLOGY_HSDPA))
    return kNetworkTechnologyHspa;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_UMTS)
    return kNetworkTechnologyUmts;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_EDGE)
    return kNetworkTechnologyEdge;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_GPRS)
    return kNetworkTechnologyGprs;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT |
                              MM_MODEM_ACCESS_TECHNOLOGY_GSM))
      return kNetworkTechnologyGsm;
  return "";
}

string AccessTechnologyToTechnologyFamily(uint32 access_technologies) {
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_LTE |
                             MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS |
                             MM_MODEM_ACCESS_TECHNOLOGY_HSPA |
                             MM_MODEM_ACCESS_TECHNOLOGY_HSUPA |
                             MM_MODEM_ACCESS_TECHNOLOGY_HSDPA |
                             MM_MODEM_ACCESS_TECHNOLOGY_UMTS |
                             MM_MODEM_ACCESS_TECHNOLOGY_EDGE |
                             MM_MODEM_ACCESS_TECHNOLOGY_GPRS |
                             MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT |
                             MM_MODEM_ACCESS_TECHNOLOGY_GSM))
    return kTechnologyFamilyGsm;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_EVDO0 |
                             MM_MODEM_ACCESS_TECHNOLOGY_EVDOA |
                             MM_MODEM_ACCESS_TECHNOLOGY_EVDOB |
                             MM_MODEM_ACCESS_TECHNOLOGY_1XRTT))
    return kTechnologyFamilyCdma;
  return "";
}

}  // namespace

CellularCapabilityUniversal::CellularCapabilityUniversal(
    Cellular *cellular,
    ProxyFactory *proxy_factory,
    ModemInfo *modem_info)
    : CellularCapability(cellular, proxy_factory, modem_info),
      weak_ptr_factory_(this),
      registration_state_(MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN),
      current_capabilities_(MM_MODEM_CAPABILITY_NONE),
      access_technologies_(MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN),
      home_provider_info_(NULL),
      resetting_(false),
      subscription_state_(kSubscriptionStateUnknown),
      reset_done_(false),
      activation_registration_timeout_milliseconds_(
          kActivationRegistrationTimeoutMilliseconds),
      registration_dropped_update_timeout_milliseconds_(
          kRegistrationDroppedUpdateTimeoutMilliseconds) {
  SLOG(Cellular, 2) << "Cellular capability constructed: Universal";
  HelpRegisterConstDerivedKeyValueStore(
      kSIMLockStatusProperty,
      &CellularCapabilityUniversal::SimLockStatusToProperty);
}

KeyValueStore CellularCapabilityUniversal::SimLockStatusToProperty(
    Error */*error*/) {
  KeyValueStore status;
  string lock_type;
  switch (sim_lock_status_.lock_type) {
    case MM_MODEM_LOCK_SIM_PIN:
      lock_type = "sim-pin";
      break;
    case MM_MODEM_LOCK_SIM_PUK:
      lock_type = "sim-puk";
      break;
    default:
      lock_type = "";
      break;
  }
  status.SetBool(kSIMLockEnabledProperty, sim_lock_status_.enabled);
  status.SetString(kSIMLockTypeProperty, lock_type);
  status.SetUint(kSIMLockRetriesLeftProperty, sim_lock_status_.retries_left);
  return status;
}

void CellularCapabilityUniversal::HelpRegisterConstDerivedKeyValueStore(
    const string &name,
    KeyValueStore(CellularCapabilityUniversal::*get)(Error *error)) {
  cellular()->mutable_store()->RegisterDerivedKeyValueStore(
      name,
      KeyValueStoreAccessor(
          new CustomAccessor<CellularCapabilityUniversal, KeyValueStore>(
              this, get, NULL)));
}

void CellularCapabilityUniversal::InitProxies() {
  modem_3gpp_proxy_.reset(
      proxy_factory()->CreateMM1ModemModem3gppProxy(cellular()->dbus_path(),
                                                    cellular()->dbus_owner()));
  modem_proxy_.reset(
      proxy_factory()->CreateMM1ModemProxy(cellular()->dbus_path(),
                                           cellular()->dbus_owner()));
  modem_simple_proxy_.reset(
      proxy_factory()->CreateMM1ModemSimpleProxy(cellular()->dbus_path(),
                                                 cellular()->dbus_owner()));

  modem_proxy_->set_state_changed_callback(
      Bind(&CellularCapabilityUniversal::OnModemStateChangedSignal,
           weak_ptr_factory_.GetWeakPtr()));
  // Do not create a SIM proxy until the device is enabled because we
  // do not yet know the object path of the sim object.
  // TODO(jglasgow): register callbacks
}

void CellularCapabilityUniversal::StartModem(Error *error,
                                             const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  InitProxies();
  deferred_enable_modem_callback_.Reset();
  EnableModem(true, error, callback);
}

void CellularCapabilityUniversal::EnableModem(bool deferrable,
                                              Error *error,
                                              const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << "(deferrable=" << deferrable << ")";
  CHECK(!callback.is_null());
  Error local_error(Error::kOperationInitiated);
  modem_info()->metrics()->NotifyDeviceEnableStarted(
      cellular()->interface_index());
  modem_proxy_->Enable(
      true,
      &local_error,
      Bind(&CellularCapabilityUniversal::EnableModemCompleted,
           weak_ptr_factory_.GetWeakPtr(), deferrable, callback),
      kTimeoutEnable);
  if (local_error.IsFailure()) {
    SLOG(Cellular, 2) << __func__ << "Call to modem_proxy_->Enable() failed";
  }
  if (error) {
    error->CopyFrom(local_error);
  }
}

void CellularCapabilityUniversal::EnableModemCompleted(
    bool deferrable, const ResultCallback &callback, const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(deferrable=" << deferrable
                    << ", error=" << error << ")";

  // If the enable operation failed with Error::kWrongState, the modem is not
  // in the expected state (i.e. disabled). If |deferrable| indicates that the
  // enable operation can be deferred, we defer the operation until the modem
  // goes into the expected state (see OnModemStateChangedSignal).
  //
  // Note that when the SIM is locked, the enable operation also fails with
  // Error::kWrongState. The enable operation is deferred until the modem goes
  // into the disabled state after the SIM is unlocked. We may choose not to
  // defer the enable operation when the SIM is locked, but the UI needs to
  // trigger the enable operation after the SIM is unlocked, which is currently
  // not the case.
  if (error.IsFailure()) {
    if (!deferrable || error.type() != Error::kWrongState) {
      callback.Run(error);
      return;
    }

    if (deferred_enable_modem_callback_.is_null()) {
      SLOG(Cellular, 2) << "Defer enable operation.";
      // The Enable operation to be deferred should not be further deferrable.
      deferred_enable_modem_callback_ =
          Bind(&CellularCapabilityUniversal::EnableModem,
               weak_ptr_factory_.GetWeakPtr(),
               false,  // non-deferrable
               static_cast<Error*>(NULL),
               callback);
    }
    return;
  }

  // After modem is enabled, it should be possible to get properties
  // TODO(jglasgow): handle errors from GetProperties
  GetProperties();
  // We expect the modem to start scanning after it has been enabled.
  // Change this if this behavior is no longer the case in the future.
  modem_info()->metrics()->NotifyDeviceEnableFinished(
      cellular()->interface_index());
  modem_info()->metrics()->NotifyDeviceScanStarted(
      cellular()->interface_index());
  callback.Run(error);
}

void CellularCapabilityUniversal::StopModem(Error *error,
                                            const ResultCallback &callback) {
  CHECK(!callback.is_null());
  CHECK(error);
  // If there is an outstanding registration change, simply ignore it since
  // the service will be destroyed anyway.
  if (!registration_dropped_update_callback_.IsCancelled()) {
    registration_dropped_update_callback_.Cancel();
    SLOG(Cellular, 2) << __func__ << " Cancelled delayed deregister.";
  }

  Closure task = Bind(&CellularCapabilityUniversal::Stop_Disable,
                      weak_ptr_factory_.GetWeakPtr(),
                      callback);
  cellular()->dispatcher()->PostTask(task);
  deferred_enable_modem_callback_.Reset();
}

void CellularCapabilityUniversal::Stop_Disable(const ResultCallback &callback) {
  Error error;
  modem_info()->metrics()->NotifyDeviceDisableStarted(
      cellular()->interface_index());
  modem_proxy_->Enable(
      false, &error,
      Bind(&CellularCapabilityUniversal::Stop_DisableCompleted,
           weak_ptr_factory_.GetWeakPtr(), callback),
      kTimeoutEnable);
  if (error.IsFailure())
    callback.Run(error);
}

void CellularCapabilityUniversal::Stop_DisableCompleted(
    const ResultCallback &callback, const Error &error) {
  SLOG(Cellular, 2) << __func__;

  if (error.IsSuccess()) {
    // The modem has been successfully disabled, but we still need to power it
    // down.
    Stop_PowerDown(callback);
  } else {
    // An error occurred; terminate the disable sequence.
    callback.Run(error);
  }
}

void CellularCapabilityUniversal::Stop_PowerDown(
    const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  Error error;
  modem_proxy_->SetPowerState(
      MM_MODEM_POWER_STATE_LOW,
      &error,
      Bind(&CellularCapabilityUniversal::Stop_PowerDownCompleted,
           weak_ptr_factory_.GetWeakPtr(), callback),
      kSetPowerStateTimeoutMilliseconds);

  if (error.IsFailure())
    // This really shouldn't happen, but if it does, report success,
    // because a stop initiated power down is only called if the
    // modem was successfully disabled, but the failure of this
    // operation should still be propagated up as a successful disable.
    Stop_PowerDownCompleted(callback, error);
}

// Note: if we were in the middle of powering down the modem when the
// system suspended, we might not get this event from
// ModemManager. And we might not even get a timeout from dbus-c++,
// because StartModem re-initializes proxies.
void CellularCapabilityUniversal::Stop_PowerDownCompleted(
    const ResultCallback &callback,
    const Error &error) {
  SLOG(Cellular, 2) << __func__;

  if (error.IsFailure())
    SLOG(Cellular, 2) << "Ignoring error returned by SetPowerState: " << error;

  // Since the disable succeeded, if power down fails, we currently fail
  // silently, i.e. we need to report the disable operation as having
  // succeeded.
  modem_info()->metrics()->NotifyDeviceDisableFinished(
      cellular()->interface_index());
  ReleaseProxies();
  callback.Run(Error());
}

void CellularCapabilityUniversal::Connect(const DBusPropertiesMap &properties,
                                          Error *error,
                                          const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  DBusPathCallback cb = Bind(&CellularCapabilityUniversal::OnConnectReply,
                             weak_ptr_factory_.GetWeakPtr(),
                             callback);
  modem_simple_proxy_->Connect(properties, error, cb, kTimeoutConnect);
}

void CellularCapabilityUniversal::Disconnect(Error *error,
                                             const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  // If a deferred registration loss request exists, process it.
  if (!registration_dropped_update_callback_.IsCancelled()) {
    registration_dropped_update_callback_.callback().Run();
    DCHECK(cellular()->state() != Cellular::kStateConnected &&
           cellular()->state() != Cellular::kStateLinked);
    SLOG(Cellular, 1) << "Processed deferred registration loss before "
                      << "disconnect request.";
  }
  if (active_bearer_path_.empty()) {
    LOG(WARNING) << "In " << __func__ << "(): "
                 << "Ignoring attempt to disconnect without bearer";
  } else if (modem_simple_proxy_.get()) {
    SLOG(Cellular, 2) << "Disconnect bearer " << active_bearer_path_;
    modem_simple_proxy_->Disconnect(active_bearer_path_,
                                    error,
                                    callback,
                                    kTimeoutDisconnect);
  }
}

void CellularCapabilityUniversal::DisconnectCleanup() {
  SLOG(Cellular, 2) << __func__;
}

void CellularCapabilityUniversal::Activate(const string &carrier,
                                           Error *error,
                                           const ResultCallback &callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapabilityUniversal::OnActivationWaitForRegisterTimeout() {
  SLOG(Cellular, 2) << __func__;
  const string &sim_identifier = cellular()->sim_identifier();
  if (sim_identifier.empty() ||
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierICCID,
          sim_identifier) == PendingActivationStore::kStateActivated) {
    SLOG(Cellular, 2) << "Modem is already scheduled to be reset.";
    return;
  }
  if (IsMdnValid()) {
    SLOG(Cellular, 2) << "MDN is valid. Already activated.";
    return;
  }
  if (reset_done_) {
    SLOG(Cellular, 2) << "Already done with reset.";
    return;
  }

  // Still not activated after timeout. Reset the modem.
  SLOG(Cellular, 2) << "Still not registered after timeout. Reset directly "
                    << "to update MDN.";
  modem_info()->pending_activation_store()->SetActivationState(
      PendingActivationStore::kIdentifierICCID,
      sim_identifier,
      PendingActivationStore::kStatePendingTimeout);
  ResetAfterActivation();
}

void CellularCapabilityUniversal::CompleteActivation(Error *error) {
  SLOG(Cellular, 2) << __func__;

  // Persist the ICCID as "Pending Activation".
  // We're assuming that when this function gets called,
  // |cellular()->sim_identifier()| will be non-empty. We still check here that
  // is non-empty, though something is wrong if it is empty.
  const string &sim_identifier = cellular()->sim_identifier();
  if (sim_identifier.empty()) {
    SLOG(Cellular, 2) << "SIM identifier not available. Nothing to do.";
    return;
  }

  if (IsMdnValid()) {
    SLOG(Cellular, 2) << "Already acquired a valid MDN. Already activated.";
    return;
  }

  // There should be a cellular service at this point.
  if (cellular()->service().get()) {
    if (cellular()->service()->activation_state() == kActivationStateActivated)
      return;

    cellular()->service()->SetActivationState(kActivationStateActivating);
  }
  modem_info()->pending_activation_store()->SetActivationState(
      PendingActivationStore::kIdentifierICCID,
      sim_identifier,
      PendingActivationStore::kStatePending);

  activation_wait_for_registration_callback_.Reset(
      Bind(&CellularCapabilityUniversal::OnActivationWaitForRegisterTimeout,
           weak_ptr_factory_.GetWeakPtr()));
  cellular()->dispatcher()->PostDelayedTask(
      activation_wait_for_registration_callback_.callback(),
      activation_registration_timeout_milliseconds_);
}

void CellularCapabilityUniversal::ResetAfterActivation() {
  SLOG(Cellular, 2) << __func__;

  // Here the initial call to Reset might fail in rare cases. Simply ignore.
  Error error;
  ResultCallback callback = Bind(
      &CellularCapabilityUniversal::OnResetAfterActivationReply,
      weak_ptr_factory_.GetWeakPtr());
  Reset(&error, callback);
  if (error.IsFailure())
    SLOG(Cellular, 2) << "Failed to reset after activation.";
}

void CellularCapabilityUniversal::OnResetAfterActivationReply(
    const Error &error) {
  SLOG(Cellular, 2) << __func__;
  if (error.IsFailure()) {
    SLOG(Cellular, 2) << "Failed to reset after activation. Try again later.";
    // TODO(armansito): Maybe post a delayed reset task?
    return;
  }
  reset_done_ = true;
  activation_wait_for_registration_callback_.Cancel();
  UpdatePendingActivationState();
}

void CellularCapabilityUniversal::UpdatePendingActivationState() {
  SLOG(Cellular, 2) << __func__;

  const string &sim_identifier = cellular()->sim_identifier();
  bool registered =
      registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_HOME;

  // We know a service is activated if |subscription_state_| is
  // kSubscriptionStateProvisioned / kSubscriptionStateOutOfData
  // In the case that |subscription_state_| is kSubscriptionStateUnknown, we
  // fallback on checking for a valid MDN.
  bool activated =
    ((subscription_state_ == kSubscriptionStateProvisioned) ||
     (subscription_state_ == kSubscriptionStateOutOfData)) ||
    ((subscription_state_ == kSubscriptionStateUnknown) && IsMdnValid());

  if (activated && !sim_identifier.empty())
      modem_info()->pending_activation_store()->RemoveEntry(
          PendingActivationStore::kIdentifierICCID,
          sim_identifier);

  CellularServiceRefPtr service = cellular()->service();

  if (!service.get())
    return;

  if (service->activation_state() == kActivationStateActivated)
      // Either no service or already activated. Nothing to do.
      return;

  // Besides the indicators above, a connected service also indicates an
  // activated SIM.
  if (activated || cellular()->state() == Cellular::kStateConnected ||
      cellular()->state() == Cellular::kStateLinked) {
    SLOG(Cellular, 2) << "Marking service as activated.";
    service->SetActivationState(kActivationStateActivated);
    return;
  }

  // If the ICCID is not available, the following logic can be delayed until it
  // becomes available.
  if (sim_identifier.empty())
    return;

  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierICCID,
          sim_identifier);
  switch (state) {
    case PendingActivationStore::kStatePending:
      // Always mark the service as activating here, as the ICCID could have
      // been unavailable earlier.
      service->SetActivationState(kActivationStateActivating);
      if (reset_done_) {
        SLOG(Cellular, 2) << "Post-payment activation reset complete.";
        modem_info()->pending_activation_store()->SetActivationState(
            PendingActivationStore::kIdentifierICCID,
            sim_identifier,
            PendingActivationStore::kStateActivated);
      } else if (registered) {
        SLOG(Cellular, 2) << "Resetting modem for activation.";
        activation_wait_for_registration_callback_.Cancel();
        ResetAfterActivation();
      }
      break;
    case PendingActivationStore::kStateActivated:
      if (registered) {
        // Trigger auto connect here.
        SLOG(Cellular, 2) << "Modem has been reset at least once, try to "
                          << "autoconnect to force MDN to update.";
        service->AutoConnect();
      }
      break;
    case PendingActivationStore::kStatePendingTimeout:
      SLOG(Cellular, 2) << "Modem failed to register within timeout, but has "
                        << "been reset at least once.";
      if (registered) {
        SLOG(Cellular, 2) << "Registered to network, marking as activated.";
        modem_info()->pending_activation_store()->SetActivationState(
            PendingActivationStore::kIdentifierICCID,
            sim_identifier,
            PendingActivationStore::kStateActivated);
        service->SetActivationState(kActivationStateActivated);
      }
      break;
    case PendingActivationStore::kStateUnknown:
      // No entry exists for this ICCID. Nothing to do.
      break;
    default:
      NOTREACHED();
  }
}

string CellularCapabilityUniversal::GetMdnForOLP(
    const CellularOperatorInfo::CellularOperator &cellular_operator) const {
  // TODO(benchan): This is ugly. Remove carrier specific code once we move
  // mobile activation logic to carrier-specifc extensions (crbug.com/260073).
  const string &mdn = cellular()->mdn();
  if (cellular_operator.identifier() == kVzwIdentifier) {
    // subscription_state_ is the definitive indicator of whether we need
    // activation. The OLP expects an all zero MDN in that case.
    if (subscription_state_ == kSubscriptionStateUnprovisioned || mdn.empty()) {
      return string(kVzwMdnLength, '0');
    }
    if (mdn.length() > kVzwMdnLength) {
      return mdn.substr(mdn.length() - kVzwMdnLength);
    }
  }
  return mdn;
}

void CellularCapabilityUniversal::ReleaseProxies() {
  SLOG(Cellular, 2) << __func__;
  modem_3gpp_proxy_.reset();
  modem_proxy_.reset();
  modem_simple_proxy_.reset();
  sim_proxy_.reset();
}

void CellularCapabilityUniversal::UpdateStorageIdentifier() {
  if (!cellular()->service().get())
    return;

  // Lookup the unique identifier assigned to the current network and base the
  // service's storage identifier on it.
  const string kPrefix =
      string(shill::kTypeCellular) + "_" + cellular()->address() + "_";
  string storage_id;
  if (!operator_id_.empty()) {
    const CellularOperatorInfo::CellularOperator *provider =
        modem_info()->cellular_operator_info()->GetCellularOperatorByMCCMNC(
            operator_id_);
    if (provider && !provider->identifier().empty()) {
      storage_id = kPrefix + provider->identifier();
    }
  }
  // If the above didn't work, append IMSI, if available.
  if (storage_id.empty() && !cellular()->imsi().empty()) {
    storage_id = kPrefix + cellular()->imsi();
  }
  if (!storage_id.empty()) {
    cellular()->service()->SetStorageIdentifier(storage_id);
  }
}

void CellularCapabilityUniversal::UpdateServiceActivationState() {
  if (!cellular()->service().get())
    return;

  const string &sim_identifier = cellular()->sim_identifier();
  bool activation_required = IsServiceActivationRequired();
  string activation_state;
  PendingActivationStore::State state =
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierICCID,
          sim_identifier);
  if ((subscription_state_ == kSubscriptionStateUnknown ||
       subscription_state_ == kSubscriptionStateUnprovisioned) &&
      !sim_identifier.empty() &&
      (state == PendingActivationStore::kStatePending ||
       state == PendingActivationStore::kStatePendingTimeout))
    activation_state = kActivationStateActivating;
  else if (activation_required)
    activation_state = kActivationStateNotActivated;
  else {
    activation_state = kActivationStateActivated;

    // Mark an activated service for auto-connect by default. Since data from
    // the user profile will be loaded after the call to OnServiceCreated, this
    // property will be corrected based on the user data at that time.
    cellular()->service()->SetAutoConnect(true);
  }
  cellular()->service()->SetActivationState(activation_state);
  // TODO(benchan): For now, assume the cellular service is activated over
  // a non-cellular network if service activation is required (i.e. a
  // corresponding entry is found in the cellular operator info file).
  // We will need to generalize this logic when migrating CDMA support from
  // cromo to ModemManager.
  cellular()->service()->SetActivateOverNonCellularNetwork(activation_required);
}

void CellularCapabilityUniversal::OnServiceCreated() {
  UpdateStorageIdentifier();
  UpdateServiceActivationState();
  UpdateServingOperator();
  UpdateOLP();

  // WORKAROUND:
  // E362 modems on Verizon network does not properly redirect when a SIM
  // runs out of credits, we need to enforce out-of-credits detection.
  //
  // The out-of-credits detection is also needed on ALT3100 modems until the PCO
  // support is ready (crosbug.com/p/20461).
  cellular()->service()->InitOutOfCreditsDetection(
      GetOutOfCreditsDetectionType());

  // Make sure that the network technology is set when the service gets
  // created, just in case.
  cellular()->service()->SetNetworkTechnology(GetNetworkTechnologyString());
}

// Create the list of APNs to try, in the following order:
// - last APN that resulted in a successful connection attempt on the
//   current network (if any)
// - the APN, if any, that was set by the user
// - the list of APNs found in the mobile broadband provider DB for the
//   home provider associated with the current SIM
// - as a last resort, attempt to connect with no APN
void CellularCapabilityUniversal::SetupApnTryList() {
  apn_try_list_.clear();

  DCHECK(cellular()->service().get());
  const Stringmap *apn_info = cellular()->service()->GetLastGoodApn();
  if (apn_info)
    apn_try_list_.push_back(*apn_info);

  apn_info = cellular()->service()->GetUserSpecifiedApn();
  if (apn_info)
    apn_try_list_.push_back(*apn_info);

  apn_try_list_.insert(apn_try_list_.end(),
                       cellular()->apn_list().begin(),
                       cellular()->apn_list().end());
}

void CellularCapabilityUniversal::SetupConnectProperties(
    DBusPropertiesMap *properties) {
  SetupApnTryList();
  FillConnectPropertyMap(properties);
}

void CellularCapabilityUniversal::FillConnectPropertyMap(
    DBusPropertiesMap *properties) {

  // TODO(jglasgow): Is this really needed anymore?
  (*properties)[kConnectNumber].writer().append_string(
      kPhoneNumber);

  (*properties)[kConnectAllowRoaming].writer().append_bool(
      AllowRoaming());

  if (!apn_try_list_.empty()) {
    // Leave the APN at the front of the list, so that it can be recorded
    // if the connect attempt succeeds.
    Stringmap apn_info = apn_try_list_.front();
    SLOG(Cellular, 2) << __func__ << ": Using APN " << apn_info[kApnProperty];
    (*properties)[kConnectApn].writer().append_string(
        apn_info[kApnProperty].c_str());
    if (ContainsKey(apn_info, kApnUsernameProperty))
      (*properties)[kConnectUser].writer().append_string(
          apn_info[kApnUsernameProperty].c_str());
    if (ContainsKey(apn_info, kApnPasswordProperty))
      (*properties)[kConnectPassword].writer().append_string(
          apn_info[kApnPasswordProperty].c_str());
  }
}

void CellularCapabilityUniversal::OnConnectReply(const ResultCallback &callback,
                                                 const DBus::Path &path,
                                                 const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";

  CellularServiceRefPtr service = cellular()->service();
  if (!service) {
    // The service could have been deleted before our Connect() request
    // completes if the modem was enabled and then quickly disabled.
    apn_try_list_.clear();
  } else if (error.IsFailure()) {
    service->ClearLastGoodApn();
    // The APN that was just tried (and failed) is still at the
    // front of the list, about to be removed. If the list is empty
    // after that, try one last time without an APN. This may succeed
    // with some modems in some cases.
    if (RetriableConnectError(error) && !apn_try_list_.empty()) {
      apn_try_list_.pop_front();
      SLOG(Cellular, 2) << "Connect failed with invalid APN, "
                        << apn_try_list_.size() << " remaining APNs to try";
      DBusPropertiesMap props;
      FillConnectPropertyMap(&props);
      Error error;
      Connect(props, &error, callback);
      return;
    }
  } else {
    if (!apn_try_list_.empty()) {
      service->SetLastGoodApn(apn_try_list_.front());
      apn_try_list_.clear();
    }
    active_bearer_path_ = path;
    SLOG(Cellular, 2) << "Connected bearer " << active_bearer_path_;
  }

  if (!callback.is_null())
    callback.Run(error);

  UpdatePendingActivationState();
}

bool CellularCapabilityUniversal::AllowRoaming() {
  return cellular()->provider_requires_roaming() || allow_roaming_property();
}

void CellularCapabilityUniversal::GetProperties() {
  SLOG(Cellular, 2) << __func__;

  scoped_ptr<DBusPropertiesProxyInterface> properties_proxy(
      proxy_factory()->CreateDBusPropertiesProxy(cellular()->dbus_path(),
                                                 cellular()->dbus_owner()));
  DBusPropertiesMap properties(
      properties_proxy->GetAll(MM_DBUS_INTERFACE_MODEM));
  OnModemPropertiesChanged(properties, vector<string>());

  properties = properties_proxy->GetAll(MM_DBUS_INTERFACE_MODEM_MODEM3GPP);
  OnModem3GPPPropertiesChanged(properties, vector<string>());
}

// static
string CellularCapabilityUniversal::GenerateNewGenericServiceName() {
  return base::StringPrintf("%s %u",
                            kGenericServiceNamePrefix,
                            friendly_service_name_id_++);
}

string CellularCapabilityUniversal::CreateFriendlyServiceName() {
  SLOG(Cellular, 2) << __func__ << ": " << GetRoamingStateString();

  // If |serving_operator_| does not have an operator ID, call
  // UpdateOperatorInfo() to use |operator_id_| as a fallback when appropriate.
  if (serving_operator_.GetCode().empty()) {
    UpdateOperatorInfo();
  }

  string name = serving_operator_.GetName();
  string home_provider_name = cellular()->home_provider().GetName();
  if (!name.empty()) {
    // If roaming, try to show "<home-provider> | <serving-operator>", per 3GPP
    // rules (TS 31.102 and annex A of 122.101).
    if (registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING &&
        !home_provider_name.empty()) {
      return home_provider_name + " | " + name;
    }
    return name;
  }
  if (registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_HOME &&
      !home_provider_name.empty()) {
    return home_provider_name;
  }
  if (!serving_operator_.GetCode().empty()) {
    return "cellular_" + serving_operator_.GetCode();
  }
  return GenerateNewGenericServiceName();
}

void CellularCapabilityUniversal::SetHomeProvider() {
  const string &imsi = cellular()->imsi();
  SLOG(Cellular, 2) << __func__ << "(IMSI: " << imsi
          << " SPN: " << spn_ << ")";

  if (!modem_info()->provider_db())
    return;

  // MCCMNC can be determined either from IMSI or Operator Code. Use whichever
  // one is available. If both were reported by the SIM, use IMSI.
  const string &network_id = imsi.empty() ? operator_id_ : imsi;
  mobile_provider *provider_info = mobile_provider_lookup_best_match(
      modem_info()->provider_db(),
      spn_.c_str(),
      network_id.c_str());
  if (!provider_info) {
    SLOG(Cellular, 2) << "3GPP provider not found.";
    return;
  }

  // Even if |provider_info| is the same as |home_provider_info_|, it is
  // possible that the |spn_| has changed.  Run all the code below.
  home_provider_info_ = provider_info;
  cellular()->set_provider_requires_roaming(
      home_provider_info_->requires_roaming);
  Cellular::Operator oper;
  // If Operator ID is available, use that as network code, otherwise
  // use what was returned from the database.
  if (!operator_id_.empty()) {
    oper.SetCode(operator_id_);
  } else if (provider_info->networks && provider_info->networks[0]) {
    oper.SetCode(provider_info->networks[0]);
  }
  if (provider_info->country) {
    oper.SetCountry(provider_info->country);
  }
  if (spn_.empty()) {
    const char *name = mobile_provider_get_name(provider_info);
    if (name) {
      oper.SetName(name);
    }
  } else {
    oper.SetName(spn_);
  }
  cellular()->set_home_provider(oper);
  bool roaming_required = cellular()->provider_requires_roaming();
  SLOG(Cellular, 2) << "Home provider: " << oper.GetCode() << ", "
                    << oper.GetName() << ", " << oper.GetCountry()
                    << (roaming_required ? ", roaming required" : "");
  InitAPNList();
  UpdateServiceName();
}

void CellularCapabilityUniversal::UpdateOLP() {
  SLOG(Cellular, 2) << __func__;

  // TODO(armansito): Do this mapping in MobileOperator (See crbug.com/298408).
  const CellularOperatorInfo *cellular_operator_info =
      modem_info()->cellular_operator_info();
  if (!cellular_operator_info)
    return;

  const CellularOperatorInfo::CellularOperator *cellular_operator =
      cellular_operator_info->GetCellularOperatorByMCCMNC(operator_id_);
  if (!cellular_operator)
    return;

  const CellularService::OLP *result =
      cellular_operator_info->GetOLPByMCCMNC(operator_id_);
  if (!result)
    return;

  CellularService::OLP olp;
  olp.CopyFrom(*result);
  string post_data = olp.GetPostData();
  ReplaceSubstringsAfterOffset(&post_data, 0, "${iccid}",
                               cellular()->sim_identifier());
  ReplaceSubstringsAfterOffset(&post_data, 0, "${imei}", cellular()->imei());
  ReplaceSubstringsAfterOffset(&post_data, 0, "${imsi}", cellular()->imsi());
  ReplaceSubstringsAfterOffset(&post_data, 0, "${mdn}",
                               GetMdnForOLP(*cellular_operator));
  ReplaceSubstringsAfterOffset(&post_data, 0, "${min}", cellular()->min());

  // TODO(armansito): Define constants for the OEM IDs in MobileOperator
  // (See crbug.com/298408).
  string oem_id = (cellular()->model_id() == kE362ModelId) ? "GOG3" : "QUA";
  ReplaceSubstringsAfterOffset(&post_data, 0, "${oem}", oem_id);
  olp.SetPostData(post_data);
  cellular()->service()->SetOLP(olp);
}

void CellularCapabilityUniversal::UpdateServiceName() {
  if (cellular()->service()) {
    cellular()->service()->SetFriendlyName(CreateFriendlyServiceName());
  }
}

void CellularCapabilityUniversal::UpdateOperatorInfo() {
  SLOG(Cellular, 2) << __func__;
  // TODO(armansito): Use CellularOperatorInfo here instead of
  // mobile_provider_db.

  // Sometimes the modem fails to acquire the operator code OTA, in which case
  // |serving_operator_| may not have an operator ID (sometimes due to service
  // activation being required or broken modem firmware). Use |operator_id_| as
  // a fallback when available. |operator_id_| is retrieved from the SIM card.
  if (serving_operator_.GetCode().empty() && !operator_id_.empty()) {
    SLOG(Cellular, 2) << "Assuming operator '" << operator_id_
                      << "' as serving operator.";
    serving_operator_.SetCode(operator_id_);
  } else if (!serving_operator_.GetCode().empty() && operator_id_.empty()) {
    // Sometimes the SIM may fail to report an operator code. Since we build
    // the APN list based on home provider, report that the serving operator
    // is the home provider.
    // TODO(armansito): This is clearly not the best behavior for the roaming
    // case: we are now reporting that the roaming carrier is the same as the
    // home carrier. While this is not preferable, we don't have any home
    // provider to report either way, so this won't hurt the user experience.
    // This is a quick fix needed for the M31 release (crbug.com/306310).
    // Remove it with a better approach for M32 (crbug.com/298408).
    SLOG(Cellular, 2) << "Home provider is unknown but serving operator is. "
                      << "Reporting serving operator as home provider.";
    operator_id_ = serving_operator_.GetCode();
    SetHomeProvider();
  }

  const string &network_id = serving_operator_.GetCode();
  if (!network_id.empty()) {
    SLOG(Cellular, 2) << "Looking up network id: " << network_id;
    mobile_provider *provider =
        mobile_provider_lookup_by_network(modem_info()->provider_db(),
                                          network_id.c_str());
    if (provider) {
      if (serving_operator_.GetName().empty()) {
        const char *provider_name = mobile_provider_get_name(provider);
        if (provider_name && *provider_name) {
          serving_operator_.SetName(provider_name);
        }
      }
      if (provider->country && *provider->country) {
        serving_operator_.SetCountry(provider->country);
      }
      SLOG(Cellular, 2) << "Operator name: " << serving_operator_.GetName()
                        << ", country: " << serving_operator_.GetCountry();
    } else {
      SLOG(Cellular, 2) << "GSM provider not found.";
    }
  }
  UpdateServingOperator();
}

void CellularCapabilityUniversal::UpdateServingOperator() {
  SLOG(Cellular, 2) << __func__;
  if (cellular()->service().get()) {
    cellular()->service()->SetServingOperator(serving_operator_);
  }
}

void CellularCapabilityUniversal::UpdateActiveBearerPath() {
  // Look for the first active bearer and use its path as the connected
  // one. Right now, we don't allow more than one active bearer.
  DBus::Path new_bearer_path;
  uint32 ipconfig_method(MM_BEARER_IP_METHOD_UNKNOWN);
  string network_device;

  for (const auto &path : bearer_paths_) {
    scoped_ptr<DBusPropertiesProxyInterface> properties_proxy(
        proxy_factory()->CreateDBusPropertiesProxy(path,
                                                   cellular()->dbus_owner()));
    DBusPropertiesMap properties(
        properties_proxy->GetAll(MM_DBUS_INTERFACE_BEARER));
    if (properties.empty()) {
      LOG(WARNING) << "Could not get properties of bearer \"" << path
                   << "\". Bearer is likely gone and thus ignored.";
      continue;
    }

    bool connected = false;
    if (!DBusProperties::GetBool(
            properties, MM_BEARER_PROPERTY_CONNECTED, &connected)) {
      SLOG(Cellular, 2) << "Bearer does not indicate whether it is connected "
                           "or not. Assume it is not connected.";
      continue;
    }

    if (!connected) {
      continue;
    }

    SLOG(Cellular, 2) << "Found active bearer \"" << path << "\".";
    CHECK(new_bearer_path.empty()) << "Found more than one active bearer.";

    if (DBusProperties::GetString(
            properties, MM_BEARER_PROPERTY_INTERFACE, &network_device)) {
      SLOG(Cellular, 2) << "Bearer uses network interface \"" << network_device
                        << "\".";
    } else {
      SLOG(Cellular, 2) << "Bearer does not specify network interface.";
    }

    // TODO(quiche): Add support for scenarios where the bearer is
    // IPv6 only, or where there are conflicting configuration methods
    // for IPv4 and IPv6. crbug.com/248360.
    DBusPropertiesMap bearer_ip4config;
    DBusProperties::GetDBusPropertiesMap(
        properties, MM_BEARER_PROPERTY_IP4CONFIG, &bearer_ip4config);

    if (DBusProperties::GetUint32(
            bearer_ip4config, kIpConfigPropertyMethod, &ipconfig_method)) {
      SLOG(Cellular, 2) << "Bearer has IPv4 config method " << ipconfig_method;
    } else {
      SLOG(Cellular, 2) << "Bearer does not specify IPv4 config method.";
      for (const auto &i : bearer_ip4config) {
        SLOG(Cellular, 5) << "Bearer IPv4 config has key \""
                          << i.first
                          << "\".";
      }
    }
    new_bearer_path = path;
  }
  active_bearer_path_ = new_bearer_path;
  if (new_bearer_path.empty()) {
    SLOG(Cellular, 2) << "No active bearer found.";
    return;
  }

  // TODO(benchan): Move the following code outside this method.
  if (ipconfig_method == MM_BEARER_IP_METHOD_PPP) {
    cellular()->StartPPP(network_device);
  }
}

void CellularCapabilityUniversal::InitAPNList() {
  SLOG(Cellular, 2) << __func__;
  Stringmaps apn_list;
  if (!home_provider_info_) {
    return;
  }
  for (int i = 0; i < home_provider_info_->num_apns; ++i) {
    Stringmap props;
    mobile_apn *apn = home_provider_info_->apns[i];
    if (apn->value) {
      props[kApnProperty] = apn->value;
    }
    if (apn->username) {
      props[kApnUsernameProperty] = apn->username;
    }
    if (apn->password) {
      props[kApnPasswordProperty] = apn->password;
    }
    // Find the first localized and non-localized name, if any.
    const localized_name *lname = NULL;
    const localized_name *name = NULL;
    for (int j = 0; j < apn->num_names; ++j) {
      if (apn->names[j]->lang) {
        if (!lname) {
          lname = apn->names[j];
        }
      } else if (!name) {
        name = apn->names[j];
      }
    }
    if (name) {
      props[kApnNameProperty] = name->name;
    }
    if (lname) {
      props[kApnLocalizedNameProperty] = lname->name;
      props[kApnLanguageProperty] = lname->lang;
    }
    apn_list.push_back(props);
  }
  cellular()->set_apn_list(apn_list);
}

bool CellularCapabilityUniversal::IsServiceActivationRequired() const {
  const string &sim_identifier = cellular()->sim_identifier();
  // subscription_state_ is the definitive answer. If that does not work,
  // fallback on MDN based logic.
  if (subscription_state_ == kSubscriptionStateProvisioned ||
      subscription_state_ == kSubscriptionStateOutOfData)
    return false;

  // We are in the process of activating, ignore all other clues from the
  // network and use our own knowledge about the activation state.
  if (!sim_identifier.empty() &&
      modem_info()->pending_activation_store()->GetActivationState(
          PendingActivationStore::kIdentifierICCID,
          sim_identifier) != PendingActivationStore::kStateUnknown)
    return false;

  // Network notification that the service needs to be activated.
  if (subscription_state_ == kSubscriptionStateUnprovisioned)
    return true;

  // If there is no online payment portal information, it's safer to assume
  // the service does not require activation.
  if (!modem_info()->cellular_operator_info())
    return false;

  const CellularService::OLP *olp =
      modem_info()->cellular_operator_info()->GetOLPByMCCMNC(operator_id_);
  if (!olp)
    return false;

  // If the MDN is invalid (i.e. empty or contains only zeros), the service
  // requires activation.
  return !IsMdnValid();
}

bool CellularCapabilityUniversal::IsMdnValid() const {
  const string &mdn = cellular()->mdn();
  // Note that |mdn| is normalized to contain only digits in OnMdnChanged().
  for (size_t i = 0; i < mdn.size(); ++i) {
    if (mdn[i] != '0')
      return true;
  }
  return false;
}

// always called from an async context
void CellularCapabilityUniversal::Register(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << " \"" << cellular()->selected_network()
                    << "\"";
  CHECK(!callback.is_null());
  Error error;
  ResultCallback cb = Bind(&CellularCapabilityUniversal::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Register(cellular()->selected_network(), &error, cb,
                              kTimeoutRegister);
  if (error.IsFailure())
    callback.Run(error);
}

void CellularCapabilityUniversal::RegisterOnNetwork(
    const string &network_id,
    Error *error,
    const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << "(" << network_id << ")";
  CHECK(error);
  desired_network_ = network_id;
  ResultCallback cb = Bind(&CellularCapabilityUniversal::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Register(network_id, error, cb, kTimeoutRegister);
}

void CellularCapabilityUniversal::OnRegisterReply(
    const ResultCallback &callback,
    const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";

  if (error.IsSuccess()) {
    cellular()->set_selected_network(desired_network_);
    desired_network_.clear();
    callback.Run(error);
    return;
  }
  // If registration on the desired network failed,
  // try to register on the home network.
  if (!desired_network_.empty()) {
    desired_network_.clear();
    cellular()->set_selected_network("");
    LOG(INFO) << "Couldn't register on selected network, trying home network";
    Register(callback);
    return;
  }
  callback.Run(error);
}

bool CellularCapabilityUniversal::IsRegistered() const {
  return IsRegisteredState(registration_state_);
}

bool CellularCapabilityUniversal::IsRegisteredState(
    MMModem3gppRegistrationState state) {
  return (state == MM_MODEM_3GPP_REGISTRATION_STATE_HOME ||
          state == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING);
}

void CellularCapabilityUniversal::SetUnregistered(bool searching) {
  // If we're already in some non-registered state, don't override that
  if (registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_HOME ||
          registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING) {
    registration_state_ =
        (searching ? MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING :
                     MM_MODEM_3GPP_REGISTRATION_STATE_IDLE);
  }
}

void CellularCapabilityUniversal::RequirePIN(
    const string &pin, bool require,
    Error *error, const ResultCallback &callback) {
  CHECK(error);
  sim_proxy_->EnablePin(pin, require, error, callback, kTimeoutDefault);
}

void CellularCapabilityUniversal::EnterPIN(const string &pin,
                                           Error *error,
                                           const ResultCallback &callback) {
  CHECK(error);
  SLOG(Cellular, 2) << __func__;
  sim_proxy_->SendPin(pin, error, callback, kEnterPinTimeoutMilliseconds);
}

void CellularCapabilityUniversal::UnblockPIN(const string &unblock_code,
                                             const string &pin,
                                             Error *error,
                                             const ResultCallback &callback) {
  CHECK(error);
  sim_proxy_->SendPuk(unblock_code, pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityUniversal::ChangePIN(
    const string &old_pin, const string &new_pin,
    Error *error, const ResultCallback &callback) {
  CHECK(error);
  sim_proxy_->ChangePin(old_pin, new_pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityUniversal::Reset(Error *error,
                                        const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(error);
  if (resetting_) {
    Error::PopulateAndLog(error, Error::kInProgress, "Already resetting");
    return;
  }
  ResultCallback cb = Bind(&CellularCapabilityUniversal::OnResetReply,
                           weak_ptr_factory_.GetWeakPtr(), callback);
  modem_proxy_->Reset(error, cb, kTimeoutReset);
  if (!error->IsFailure()) {
    resetting_ = true;
  }
}

void CellularCapabilityUniversal::OnResetReply(const ResultCallback &callback,
                                               const Error &error) {
  SLOG(Cellular, 2) << __func__;
  resetting_ = false;
  if (!callback.is_null())
    callback.Run(error);
}

void CellularCapabilityUniversal::Scan(
    Error *error,
    const ResultStringmapsCallback &callback) {
  DBusPropertyMapsCallback cb = Bind(&CellularCapabilityUniversal::OnScanReply,
                                     weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Scan(error, cb, kTimeoutScan);
}

void CellularCapabilityUniversal::OnScanReply(
    const ResultStringmapsCallback &callback,
    const ScanResults &results,
    const Error &error) {
  Stringmaps found_networks;
  for (const auto &result : results)
    found_networks.push_back(ParseScanResult(result));
  callback.Run(found_networks, error);
}

Stringmap CellularCapabilityUniversal::ParseScanResult(
    const ScanResult &result) {

  /* ScanResults contain the following keys:

     "status"
     A MMModem3gppNetworkAvailability value representing network
     availability status, given as an unsigned integer (signature "u").
     This key will always be present.

     "operator-long"
     Long-format name of operator, given as a string value (signature
     "s"). If the name is unknown, this field should not be present.

     "operator-short"
     Short-format name of operator, given as a string value
     (signature "s"). If the name is unknown, this field should not
     be present.

     "operator-code"
     Mobile code of the operator, given as a string value (signature
     "s"). Returned in the format "MCCMNC", where MCC is the
     three-digit ITU E.212 Mobile Country Code and MNC is the two- or
     three-digit GSM Mobile Network Code. e.g. "31026" or "310260".

     "access-technology"
     A MMModemAccessTechnology value representing the generic access
     technology used by this mobile network, given as an unsigned
     integer (signature "u").
  */
  Stringmap parsed;

  uint32 status;
  if (DBusProperties::GetUint32(result, kStatusProperty, &status)) {
    // numerical values are taken from 3GPP TS 27.007 Section 7.3.
    static const char * const kStatusString[] = {
      "unknown",    // MM_MODEM_3GPP_NETWORK_AVAILABILITY_UNKNOWN
      "available",  // MM_MODEM_3GPP_NETWORK_AVAILABILITY_AVAILABLE
      "current",    // MM_MODEM_3GPP_NETWORK_AVAILABILITY_CURRENT
      "forbidden",  // MM_MODEM_3GPP_NETWORK_AVAILABILITY_FORBIDDEN
    };
    parsed[kStatusProperty] = kStatusString[status];
  }

  uint32 tech;  // MMModemAccessTechnology
  if (DBusProperties::GetUint32(result, kOperatorAccessTechnologyProperty,
                                &tech)) {
    parsed[kTechnologyProperty] = AccessTechnologyToString(tech);
  }

  string operator_long, operator_short, operator_code;
  if (DBusProperties::GetString(result, kOperatorLongProperty, &operator_long))
    parsed[kLongNameProperty] = operator_long;
  if (DBusProperties::GetString(result, kOperatorShortProperty,
                                &operator_short))
    parsed[kShortNameProperty] = operator_short;
  if (DBusProperties::GetString(result, kOperatorCodeProperty, &operator_code))
    parsed[kNetworkIdProperty] = operator_code;

  // If the long name is not available but the network ID is, look up the long
  // name in the mobile provider database.
  if ((!ContainsKey(parsed, kLongNameProperty) ||
       parsed[kLongNameProperty].empty()) &&
      ContainsKey(parsed, kNetworkIdProperty)) {
    mobile_provider *provider =
        mobile_provider_lookup_by_network(
            modem_info()->provider_db(),
            parsed[kNetworkIdProperty].c_str());
    if (provider) {
      const char *long_name = mobile_provider_get_name(provider);
      if (long_name && *long_name) {
        parsed[kLongNameProperty] = long_name;
      }
    }
  }
  return parsed;
}

string CellularCapabilityUniversal::GetNetworkTechnologyString() const {
  // If we know that the modem is an E362, return LTE here to make sure that
  // Chrome sees LTE as the network technology even if the actual technology is
  // unknown.
  // TODO(armansito): This hack will cause the UI to display LTE even if the
  // modem doesn't support it at a given time. This might be problematic if we
  // ever want to support switching between access technologies (e.g. falling
  // back to 3G when LTE is not available).
  if (cellular()->model_id() == kE362ModelId)
    return kNetworkTechnologyLte;

  // Order is important.  Return the highest speed technology
  // TODO(jglasgow): change shill interfaces to a capability model
  return AccessTechnologyToString(access_technologies_);
}

string CellularCapabilityUniversal::GetRoamingStateString() const {
  switch (registration_state_) {
    case MM_MODEM_3GPP_REGISTRATION_STATE_HOME:
      return kRoamingStateHome;
    case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING:
      return kRoamingStateRoaming;
    default:
      break;
  }
  return kRoamingStateUnknown;
}

// TODO(armansito): Remove this method once cromo is deprecated.
void CellularCapabilityUniversal::GetSignalQuality() {
  // ModemManager always returns the cached value, so there is no need to
  // trigger an update here. The true value is updated through a property
  // change signal.
}

string CellularCapabilityUniversal::GetTypeString() const {
  return AccessTechnologyToTechnologyFamily(access_technologies_);
}

void CellularCapabilityUniversal::OnModemPropertiesChanged(
    const DBusPropertiesMap &properties,
    const vector<string> &/* invalidated_properties */) {

  // Update the bearers property before the modem state property as
  // OnModemStateChanged may call UpdateActiveBearerPath, which reads the
  // bearers property.
  RpcIdentifiers bearers;
  if (DBusProperties::GetRpcIdentifiers(properties, MM_MODEM_PROPERTY_BEARERS,
                                        &bearers)) {
    OnBearersChanged(bearers);
  }

  // This solves a bootstrapping problem: If the modem is not yet
  // enabled, there are no proxy objects associated with the capability
  // object, so modem signals like StateChanged aren't seen. By monitoring
  // changes to the State property via the ModemManager, we're able to
  // get the initialization process started, which will result in the
  // creation of the proxy objects.
  //
  // The first time we see the change to State (when the modem state
  // is Unknown), we simply update the state, and rely on the Manager to
  // enable the device when it is registered with the Manager. On subsequent
  // changes to State, we need to explicitly enable the device ourselves.
  int32 istate;
  if (DBusProperties::GetInt32(properties, MM_MODEM_PROPERTY_STATE, &istate)) {
    Cellular::ModemState state = static_cast<Cellular::ModemState>(istate);
    OnModemStateChanged(state);
  }
  DBus::Path object_path_value;
  if (DBusProperties::GetObjectPath(properties,
                                    MM_MODEM_PROPERTY_SIM, &object_path_value))
    OnSimPathChanged(object_path_value);

  DBusPropertiesMap::const_iterator it =
      properties.find(MM_MODEM_PROPERTY_SUPPORTEDCAPABILITIES);
  if (it != properties.end()) {
    const vector<uint32> &supported_capabilities = it->second;
    OnSupportedCapabilitesChanged(supported_capabilities);
  }

  uint32 uint_value;
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_CURRENTCAPABILITIES,
                                &uint_value))
    OnModemCurrentCapabilitiesChanged(uint_value);
  // not needed: MM_MODEM_PROPERTY_MAXBEARERS
  // not needed: MM_MODEM_PROPERTY_MAXACTIVEBEARERS
  string string_value;
  if (DBusProperties::GetString(properties,
                                MM_MODEM_PROPERTY_MANUFACTURER,
                                &string_value))
    cellular()->set_manufacturer(string_value);
  if (DBusProperties::GetString(properties,
                                MM_MODEM_PROPERTY_MODEL,
                                &string_value))
    cellular()->set_model_id(string_value);
  if (DBusProperties::GetString(properties,
                               MM_MODEM_PROPERTY_REVISION,
                               &string_value))
    OnModemRevisionChanged(string_value);
  // not needed: MM_MODEM_PROPERTY_DEVICEIDENTIFIER
  // not needed: MM_MODEM_PROPERTY_DEVICE
  // not needed: MM_MODEM_PROPERTY_DRIVER
  // not needed: MM_MODEM_PROPERTY_PLUGIN
  // not needed: MM_MODEM_PROPERTY_EQUIPMENTIDENTIFIER

  // Unlock required and SimLock
  uint32 unlock_required;  // This is really of type MMModemLock
  bool lock_status_changed = false;
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_UNLOCKREQUIRED,
                                &unlock_required)) {
    OnLockTypeChanged(static_cast<MMModemLock>(unlock_required));
    lock_status_changed = true;
  }

  // Unlock retries
  it = properties.find(MM_MODEM_PROPERTY_UNLOCKRETRIES);
  if (it != properties.end()) {
    LockRetryData lock_retries = it->second.operator LockRetryData();
    OnLockRetriesChanged(lock_retries);
    lock_status_changed = true;
  }

  if (lock_status_changed)
    OnSimLockStatusChanged();

  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_ACCESSTECHNOLOGIES,
                                &uint_value))
    OnAccessTechnologiesChanged(uint_value);

  it = properties.find(MM_MODEM_PROPERTY_SIGNALQUALITY);
  if (it != properties.end()) {
    DBus::Struct<unsigned int, bool> quality = it->second;
    OnSignalQualityChanged(quality._1);
  }
  vector<string> numbers;
  if (DBusProperties::GetStrings(properties, MM_MODEM_PROPERTY_OWNNUMBERS,
                                 &numbers)) {
    string mdn;
    if (numbers.size() > 0)
      mdn = numbers[0];
    OnMdnChanged(mdn);
  }

  it = properties.find(MM_MODEM_PROPERTY_SUPPORTEDMODES);
  if (it != properties.end()) {
    const vector<DBus::Struct<uint32, uint32>> &mm_supported_modes = it->second;
    vector<ModemModes> supported_modes;
    for (const auto &modes : mm_supported_modes) {
      supported_modes.push_back(
          ModemModes(modes._1, static_cast<MMModemMode>(modes._2)));
    }
    OnSupportedModesChanged(supported_modes);
  }

  it = properties.find(MM_MODEM_PROPERTY_CURRENTMODES);
  if (it != properties.end()) {
    const DBus::Struct<uint32, uint32> &current_modes = it->second;
    OnCurrentModesChanged(ModemModes(
        current_modes._1, static_cast<MMModemMode>(current_modes._2)));
  }

  // au: MM_MODEM_PROPERTY_SUPPORTEDBANDS,
  // au: MM_MODEM_PROPERTY_BANDS
}

void CellularCapabilityUniversal::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  SLOG(Cellular, 2) << __func__ << "(" << interface << ")";
  if (interface == MM_DBUS_INTERFACE_MODEM) {
    OnModemPropertiesChanged(changed_properties, invalidated_properties);
  }
  if (interface == MM_DBUS_INTERFACE_MODEM_MODEM3GPP) {
    OnModem3GPPPropertiesChanged(changed_properties, invalidated_properties);
  }
  if (interface == MM_DBUS_INTERFACE_SIM) {
    OnSimPropertiesChanged(changed_properties, invalidated_properties);
  }
}

bool CellularCapabilityUniversal::RetriableConnectError(
    const Error &error) const {
  if (error.type() == Error::kInvalidApn)
    return true;

  // modemmanager does not ever return kInvalidApn for E362 modems
  // with 1.41 firmware.  It remains to be seem if this will change
  // with 3.x firmware.
  if ((cellular()->model_id() == kE362ModelId) &&
      (error.type() == Error::kOperationFailed))
    return true;

  return false;
}

void CellularCapabilityUniversal::OnNetworkModeSignal(uint32 /*mode*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

bool CellularCapabilityUniversal::IsValidSimPath(const string &sim_path) const {
  return !sim_path.empty() && sim_path != kRootPath;
}

string CellularCapabilityUniversal::NormalizeMdn(const string &mdn) const {
  string normalized_mdn;
  for (size_t i = 0; i < mdn.size(); ++i) {
    if (IsAsciiDigit(mdn[i]))
      normalized_mdn += mdn[i];
  }
  return normalized_mdn;
}

void CellularCapabilityUniversal::OnSimPathChanged(
    const string &sim_path) {
  if (sim_path == sim_path_)
    return;

  mm1::SimProxyInterface *proxy = NULL;
  if (IsValidSimPath(sim_path))
    proxy = proxy_factory()->CreateSimProxy(sim_path,
                                            cellular()->dbus_owner());
  sim_path_ = sim_path;
  sim_proxy_.reset(proxy);

  if (!IsValidSimPath(sim_path)) {
    // Clear all data about the sim
    cellular()->set_imsi("");
    spn_ = "";
    cellular()->set_sim_present(false);
    OnSimIdentifierChanged("");
    OnOperatorIdChanged("");
  } else {
    cellular()->set_sim_present(true);
    scoped_ptr<DBusPropertiesProxyInterface> properties_proxy(
        proxy_factory()->CreateDBusPropertiesProxy(sim_path,
                                                   cellular()->dbus_owner()));
    // TODO(jglasgow): convert to async interface
    DBusPropertiesMap properties(
        properties_proxy->GetAll(MM_DBUS_INTERFACE_SIM));
    OnSimPropertiesChanged(properties, vector<string>());
  }
}

void CellularCapabilityUniversal::OnSupportedCapabilitesChanged(
    const vector<uint32> &supported_capabilities) {
  supported_capabilities_ = supported_capabilities;
}

void CellularCapabilityUniversal::OnModemCurrentCapabilitiesChanged(
    uint32 current_capabilities) {
  current_capabilities_ = current_capabilities;

  // Only allow network scan when the modem's current capabilities support
  // GSM/UMTS.
  //
  // TODO(benchan): We should consider having the modem plugins in ModemManager
  // reporting whether network scan is supported.
  cellular()->set_scanning_supported(
      (current_capabilities & MM_MODEM_CAPABILITY_GSM_UMTS) != 0);
}

void CellularCapabilityUniversal::OnMdnChanged(
    const string &mdn) {
  cellular()->set_mdn(NormalizeMdn(mdn));
  UpdatePendingActivationState();
}

void CellularCapabilityUniversal::OnModemRevisionChanged(
    const string &revision) {
  cellular()->set_firmware_revision(revision);
}

void CellularCapabilityUniversal::OnModemStateChanged(
    Cellular::ModemState state) {
  SLOG(Cellular, 3) << __func__ << ": " << Cellular::GetModemStateString(state);

  cellular()->OnModemStateChanged(state);
  // TODO(armansito): Move the deferred enable logic to Cellular
  // (See crbug.com/279499).
  if (!deferred_enable_modem_callback_.is_null() &&
      state == Cellular::kModemStateDisabled) {
    SLOG(Cellular, 2) << "Enabling modem after deferring.";
    deferred_enable_modem_callback_.Run();
    deferred_enable_modem_callback_.Reset();
  } else if (state == Cellular::kModemStateConnected) {
    // This assumes that ModemManager updates the Bearers list and the Bearer
    // properties before changing Modem state to Connected.
    //
    // TODO(benchan): Instead of explicitly querying the properties of bearers
    // to determine the active bearer, refactor the bearer related code into a
    // proper bearer class that observes DBus properties changes.
    SLOG(Cellular, 2) << "Updating bearer path to reflect the active bearer.";
    UpdateActiveBearerPath();
  }
}

void CellularCapabilityUniversal::OnAccessTechnologiesChanged(
    uint32 access_technologies) {
  if (access_technologies_ != access_technologies) {
    const string old_type_string(GetTypeString());
    access_technologies_ = access_technologies;
    const string new_type_string(GetTypeString());
    if (new_type_string != old_type_string) {
      // TODO(jglasgow): address layering violation of emitting change
      // signal here for a property owned by Cellular.
      cellular()->adaptor()->EmitStringChanged(
          kTechnologyFamilyProperty, new_type_string);
    }
    if (cellular()->service().get()) {
      cellular()->service()->SetNetworkTechnology(GetNetworkTechnologyString());
    }
  }
}

void CellularCapabilityUniversal::OnSupportedModesChanged(
    const vector<ModemModes> &supported_modes) {
  supported_modes_ = supported_modes;
}

void CellularCapabilityUniversal::OnCurrentModesChanged(
    const ModemModes &current_modes) {
  current_modes_ = current_modes;
}

void CellularCapabilityUniversal::OnBearersChanged(
    const RpcIdentifiers &bearers) {
  bearer_paths_ = bearers;
}

void CellularCapabilityUniversal::OnLockRetriesChanged(
    const LockRetryData &lock_retries) {
  SLOG(Cellular, 2) << __func__;

  // Look for the retries left for the current lock. Try the obtain the count
  // that matches the current count. If no count for the current lock is
  // available, report the first one in the dictionary.
  LockRetryData::const_iterator it =
      lock_retries.find(sim_lock_status_.lock_type);
  if (it == lock_retries.end())
      it = lock_retries.begin();
  if (it != lock_retries.end())
    sim_lock_status_.retries_left = it->second;
  else
    // Unknown, use 999
    sim_lock_status_.retries_left = 999;
}

void CellularCapabilityUniversal::OnLockTypeChanged(
    MMModemLock lock_type) {
  SLOG(Cellular, 2) << __func__ << ": " << lock_type;
  sim_lock_status_.lock_type = lock_type;

  // If the SIM is in a locked state |sim_lock_status_.enabled| might be false.
  // This is because the corresponding property 'EnabledFacilityLocks' is on
  // the 3GPP interface and the 3GPP interface is not available while the Modem
  // is in the 'LOCKED' state.
  if (lock_type != MM_MODEM_LOCK_NONE &&
      lock_type != MM_MODEM_LOCK_UNKNOWN &&
      !sim_lock_status_.enabled)
    sim_lock_status_.enabled = true;
}

void CellularCapabilityUniversal::OnSimLockStatusChanged() {
  SLOG(Cellular, 2) << __func__;
  cellular()->adaptor()->EmitKeyValueStoreChanged(
      kSIMLockStatusProperty, SimLockStatusToProperty(NULL));

  // If the SIM is currently unlocked, assume that we need to refresh
  // carrier information, since a locked SIM prevents shill from obtaining
  // the necessary data to establish a connection later (e.g. IMSI).
  if (IsValidSimPath(sim_path_) &&
      (sim_lock_status_.lock_type == MM_MODEM_LOCK_NONE ||
       sim_lock_status_.lock_type == MM_MODEM_LOCK_UNKNOWN)) {
    scoped_ptr<DBusPropertiesProxyInterface> properties_proxy(
        proxy_factory()->CreateDBusPropertiesProxy(sim_path_,
                                                   cellular()->dbus_owner()));
    DBusPropertiesMap properties(
        properties_proxy->GetAll(MM_DBUS_INTERFACE_SIM));
    OnSimPropertiesChanged(properties, vector<string>());
  }
}

void CellularCapabilityUniversal::OnModem3GPPPropertiesChanged(
    const DBusPropertiesMap &properties,
    const vector<string> &/* invalidated_properties */) {
  SLOG(Cellular, 2) << __func__;
  uint32 uint_value;
  string imei;
  if (DBusProperties::GetString(properties,
                                MM_MODEM_MODEM3GPP_PROPERTY_IMEI,
                                &imei))
    cellular()->set_imei(imei);

  // Handle registration state changes as a single change
  string operator_code = serving_operator_.GetCode();
  string operator_name = serving_operator_.GetName();
  MMModem3gppRegistrationState state = registration_state_;
  bool registration_changed = false;
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_MODEM3GPP_PROPERTY_REGISTRATIONSTATE,
                                &uint_value)) {
    state = static_cast<MMModem3gppRegistrationState>(uint_value);
    registration_changed = true;
  }
  if (DBusProperties::GetString(properties,
                                MM_MODEM_MODEM3GPP_PROPERTY_OPERATORCODE,
                                &operator_code))
    registration_changed = true;
  if (DBusProperties::GetString(properties,
                                MM_MODEM_MODEM3GPP_PROPERTY_OPERATORNAME,
                                &operator_name))
    registration_changed = true;
  if (registration_changed)
    On3GPPRegistrationChanged(state, operator_code, operator_name);
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_MODEM3GPP_PROPERTY_SUBSCRIPTIONSTATE,
                                &uint_value))
    On3GPPSubscriptionStateChanged(
        static_cast<MMModem3gppSubscriptionState>(uint_value));

  uint32 subscription_state;
  CellularServiceRefPtr service = cellular()->service();
  if (service.get() &&
      DBusProperties::GetUint32(properties,
                                MM_MODEM_MODEM3GPP_PROPERTY_SUBSCRIPTIONSTATE,
                                &subscription_state)) {
    SLOG(Cellular, 2) << __func__ << ": Subscription state = "
                                  << subscription_state;
    service->out_of_credits_detector()->NotifySubscriptionStateChanged(
        subscription_state);
  }

  uint32 locks = 0;
  if (DBusProperties::GetUint32(
          properties, MM_MODEM_MODEM3GPP_PROPERTY_ENABLEDFACILITYLOCKS,
          &locks))
    OnFacilityLocksChanged(locks);
}

void CellularCapabilityUniversal::On3GPPRegistrationChanged(
    MMModem3gppRegistrationState state,
    const string &operator_code,
    const string &operator_name) {
  SLOG(Cellular, 2) << __func__ << ": regstate=" << state
                    << ", opercode=" << operator_code
                    << ", opername=" << operator_name;

  // While the modem is connected, if the state changed from a registered state
  // to a non registered state, defer the state change by 15 seconds.
  if (cellular()->modem_state() == Cellular::kModemStateConnected &&
      IsRegistered() && !IsRegisteredState(state)) {
    if (!registration_dropped_update_callback_.IsCancelled()) {
      LOG(WARNING) << "Modem reported consecutive 3GPP registration drops. "
                   << "Ignoring earlier notifications.";
      registration_dropped_update_callback_.Cancel();
    } else {
      // This is not a repeated post. So, count this instance of delayed drop
      // posted.
      modem_info()->metrics()->Notify3GPPRegistrationDelayedDropPosted();
    }
    SLOG(Cellular, 2) << "Posted deferred registration state update";
    registration_dropped_update_callback_.Reset(
        Bind(&CellularCapabilityUniversal::Handle3GPPRegistrationChange,
             weak_ptr_factory_.GetWeakPtr(),
             state,
             operator_code,
             operator_name));
    cellular()->dispatcher()->PostDelayedTask(
        registration_dropped_update_callback_.callback(),
        registration_dropped_update_timeout_milliseconds_);
  } else {
    if (!registration_dropped_update_callback_.IsCancelled()) {
      SLOG(Cellular, 2) << "Cancelled a deferred registration state update";
      registration_dropped_update_callback_.Cancel();
      // If we cancelled the callback here, it means we had flaky network for a
      // small duration.
      modem_info()->metrics()->Notify3GPPRegistrationDelayedDropCanceled();
    }
    Handle3GPPRegistrationChange(state, operator_code, operator_name);
  }
}

void CellularCapabilityUniversal::Handle3GPPRegistrationChange(
    MMModem3gppRegistrationState updated_state,
    string updated_operator_code,
    string updated_operator_name) {
  // A finished callback does not qualify as a canceled callback.
  // We test for a canceled callback to check for outstanding callbacks.
  // So, explicitly cancel the callback here.
  registration_dropped_update_callback_.Cancel();

  SLOG(Cellular, 2) << __func__ << ": regstate=" << updated_state
                    << ", opercode=" << updated_operator_code
                    << ", opername=" << updated_operator_name;

  registration_state_ = updated_state;
  serving_operator_.SetCode(updated_operator_code);
  serving_operator_.SetName(updated_operator_name);

  // Update the carrier name for |serving_operator_|.
  UpdateOperatorInfo();

  cellular()->HandleNewRegistrationState();

  // Update the user facing name of the cellular service.
  UpdateServiceName();

  // If the modem registered with the network and the current ICCID is pending
  // activation, then reset the modem.
  UpdatePendingActivationState();
}

void CellularCapabilityUniversal::On3GPPSubscriptionStateChanged(
    MMModem3gppSubscriptionState updated_state) {
  SLOG(Cellular, 2) << __func__ << ": Updated subscription state = "
                    << updated_state;

  // A one-to-one enum mapping.
  switch (updated_state) {
    case MM_MODEM_3GPP_SUBSCRIPTION_STATE_UNKNOWN:
      subscription_state_ = kSubscriptionStateUnknown;
      break;
    case MM_MODEM_3GPP_SUBSCRIPTION_STATE_PROVISIONED:
      subscription_state_ = kSubscriptionStateProvisioned;
      break;
    case MM_MODEM_3GPP_SUBSCRIPTION_STATE_UNPROVISIONED:
      subscription_state_ = kSubscriptionStateUnprovisioned;
      break;
    case MM_MODEM_3GPP_SUBSCRIPTION_STATE_OUT_OF_DATA:
      subscription_state_ = kSubscriptionStateOutOfData;
      break;
    default:
      LOG(ERROR) << "Unrecognized MMModem3gppSubscriptionState: "
                 << updated_state;
      subscription_state_ = kSubscriptionStateUnknown;
      return;
  }

  UpdateServiceActivationState();
  UpdatePendingActivationState();
}

void CellularCapabilityUniversal::OnModemStateChangedSignal(
    int32 old_state, int32 new_state, uint32 reason) {
  Cellular::ModemState old_modem_state =
      static_cast<Cellular::ModemState>(old_state);
  Cellular::ModemState new_modem_state =
      static_cast<Cellular::ModemState>(new_state);
  SLOG(Cellular, 2) << __func__ << "("
                    << Cellular::GetModemStateString(old_modem_state) << ", "
                    << Cellular::GetModemStateString(new_modem_state) << ", "
                    << reason << ")";
}

void CellularCapabilityUniversal::OnSignalQualityChanged(uint32 quality) {
  cellular()->HandleNewSignalQuality(quality);
}

void CellularCapabilityUniversal::OnFacilityLocksChanged(uint32 locks) {
  bool sim_enabled = !!(locks & MM_MODEM_3GPP_FACILITY_SIM);
  if (sim_lock_status_.enabled != sim_enabled) {
    sim_lock_status_.enabled = sim_enabled;
    OnSimLockStatusChanged();
  }
}

void CellularCapabilityUniversal::OnSimPropertiesChanged(
    const DBusPropertiesMap &props,
    const vector<string> &/* invalidated_properties */) {
  SLOG(Cellular, 2) << __func__;
  string value;
  if (DBusProperties::GetString(props, MM_SIM_PROPERTY_SIMIDENTIFIER, &value))
    OnSimIdentifierChanged(value);
  if (DBusProperties::GetString(props, MM_SIM_PROPERTY_OPERATORIDENTIFIER,
                                &value))
    OnOperatorIdChanged(value);
  if (DBusProperties::GetString(props, MM_SIM_PROPERTY_OPERATORNAME, &value))
    OnSpnChanged(value);
  if (DBusProperties::GetString(props, MM_SIM_PROPERTY_IMSI, &value))
    cellular()->set_imsi(value);
  SetHomeProvider();
}

void CellularCapabilityUniversal::OnSpnChanged(const std::string &spn) {
  spn_ = spn;
}

void CellularCapabilityUniversal::OnSimIdentifierChanged(const string &id) {
  cellular()->set_sim_identifier(id);
  UpdatePendingActivationState();
}

void CellularCapabilityUniversal::OnOperatorIdChanged(
    const string &operator_id) {
  SLOG(Cellular, 2) << "Operator ID = '" << operator_id << "'";
  operator_id_ = operator_id;
}

OutOfCreditsDetector::OOCType
CellularCapabilityUniversal::GetOutOfCreditsDetectionType() const {
  const string &model_id = cellular()->model_id();
  if (model_id == kALT3100ModelId) {
    return OutOfCreditsDetector::OOCTypeSubscriptionState;
  } else if (model_id == kE362ModelId) {
    return OutOfCreditsDetector::OOCTypeActivePassive;
  } else {
    return OutOfCreditsDetector::OOCTypeNone;
  }
}

}  // namespace shill
