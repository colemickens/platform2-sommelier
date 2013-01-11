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

#include "shill/activating_iccid_store.h"
#include "shill/adaptor_interfaces.h"
#include "shill/cellular_operator_info.h"
#include "shill/cellular_service.h"
#include "shill/dbus_properties_proxy_interface.h"
#include "shill/error.h"
#include "shill/logging.h"
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
const char CellularCapabilityUniversal::kConnectBands[] = "bands";
const char CellularCapabilityUniversal::kConnectAllowedModes[] =
    "allowed-modes";
const char CellularCapabilityUniversal::kConnectPreferredMode[] =
    "preferred-mode";
const char CellularCapabilityUniversal::kConnectApn[] = "apn";
const char CellularCapabilityUniversal::kConnectIPType[] = "ip-type";
const char CellularCapabilityUniversal::kConnectUser[] = "user";
const char CellularCapabilityUniversal::kConnectPassword[] = "password";
const char CellularCapabilityUniversal::kConnectNumber[] = "number";
const char CellularCapabilityUniversal::kConnectAllowRoaming[] =
    "allow-roaming";
const char CellularCapabilityUniversal::kConnectRMProtocol[] = "rm-protocol";
const int64
CellularCapabilityUniversal::kDefaultScanningOrSearchingTimeoutMilliseconds =
    60000;
const int64
CellularCapabilityUniversal::kActivationRegistrationTimeoutMilliseconds =
    20000;
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
const char CellularCapabilityUniversal::kE362ModelId[] = "E362 WWAN";
const int CellularCapabilityUniversal::kSetPowerStateTimeoutMilliseconds =
    20000;
unsigned int CellularCapabilityUniversal::friendly_service_name_id_ = 0;


static const char kPhoneNumber[] = "*99#";

static string AccessTechnologyToString(uint32 access_technologies) {
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_LTE)
    return flimflam::kNetworkTechnologyLte;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_EVDO0 |
                              MM_MODEM_ACCESS_TECHNOLOGY_EVDOA |
                              MM_MODEM_ACCESS_TECHNOLOGY_EVDOB))
    return flimflam::kNetworkTechnologyEvdo;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_1XRTT)
    return flimflam::kNetworkTechnology1Xrtt;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS)
    return flimflam::kNetworkTechnologyHspaPlus;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_HSPA |
                              MM_MODEM_ACCESS_TECHNOLOGY_HSUPA |
                              MM_MODEM_ACCESS_TECHNOLOGY_HSDPA))
    return flimflam::kNetworkTechnologyHspa;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_UMTS)
    return flimflam::kNetworkTechnologyUmts;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_EDGE)
    return flimflam::kNetworkTechnologyEdge;
  if (access_technologies & MM_MODEM_ACCESS_TECHNOLOGY_GPRS)
    return flimflam::kNetworkTechnologyGprs;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT |
                              MM_MODEM_ACCESS_TECHNOLOGY_GSM))
      return flimflam::kNetworkTechnologyGsm;
  return "";
}

static string AccessTechnologyToTechnologyFamily(uint32 access_technologies) {
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
    return flimflam::kTechnologyFamilyGsm;
  if (access_technologies & (MM_MODEM_ACCESS_TECHNOLOGY_EVDO0 |
                             MM_MODEM_ACCESS_TECHNOLOGY_EVDOA |
                             MM_MODEM_ACCESS_TECHNOLOGY_EVDOB |
                             MM_MODEM_ACCESS_TECHNOLOGY_1XRTT))
    return flimflam::kTechnologyFamilyCdma;
  return "";
}

CellularCapabilityUniversal::CellularCapabilityUniversal(
    Cellular *cellular,
    ProxyFactory *proxy_factory,
    ModemInfo *modem_info)
    : CellularCapability(cellular, proxy_factory, modem_info),
      weak_ptr_factory_(this),
      registration_state_(MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN),
      capabilities_(MM_MODEM_CAPABILITY_NONE),
      current_capabilities_(MM_MODEM_CAPABILITY_NONE),
      access_technologies_(MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN),
      supported_modes_(MM_MODEM_MODE_NONE),
      allowed_modes_(MM_MODEM_MODE_NONE),
      preferred_mode_(MM_MODEM_MODE_NONE),
      home_provider_(NULL),
      provider_requires_roaming_(false),
      resetting_(false),
      scanning_supported_(false),
      scanning_(false),
      scanning_or_searching_(false),
      scan_interval_(0),
      sim_present_(false),
      reset_done_(false),
      scanning_or_searching_timeout_milliseconds_(
          kDefaultScanningOrSearchingTimeoutMilliseconds),
      activation_registration_timeout_milliseconds_(
          kActivationRegistrationTimeoutMilliseconds) {
  SLOG(Cellular, 2) << "Cellular capability constructed: Universal";
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
  store->RegisterConstString(flimflam::kIccidProperty, &sim_identifier_);
  store->RegisterConstString(flimflam::kManufacturerProperty, &manufacturer_);
  store->RegisterConstString(flimflam::kMdnProperty, &mdn_);
  store->RegisterConstString(flimflam::kMeidProperty, &meid_);
  store->RegisterConstString(flimflam::kMinProperty, &min_);
  store->RegisterConstString(flimflam::kModelIDProperty, &model_id_);
  store->RegisterConstString(flimflam::kSelectedNetworkProperty,
                             &selected_network_);
  store->RegisterConstStringmaps(flimflam::kFoundNetworksProperty,
                                 &found_networks_);
  store->RegisterConstBool(shill::kProviderRequiresRoamingProperty,
                           &provider_requires_roaming_);
  store->RegisterConstBool(flimflam::kScanningProperty,
                           &scanning_or_searching_);
  store->RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);
  HelpRegisterDerivedKeyValueStore(
      flimflam::kSIMLockStatusProperty,
      &CellularCapabilityUniversal::SimLockStatusToProperty,
      NULL);
  store->RegisterConstBool(shill::kSIMPresentProperty, &sim_present_);
  store->RegisterConstStringmaps(flimflam::kCellularApnListProperty,
                                 &apn_list_);
}

KeyValueStore CellularCapabilityUniversal::SimLockStatusToProperty(
    Error */*error*/) {
  KeyValueStore status;
  status.SetBool(flimflam::kSIMLockEnabledProperty, sim_lock_status_.enabled);
  status.SetString(flimflam::kSIMLockTypeProperty, sim_lock_status_.lock_type);
  status.SetUint(flimflam::kSIMLockRetriesLeftProperty,
                 sim_lock_status_.retries_left);
  return status;
}

void CellularCapabilityUniversal::HelpRegisterDerivedKeyValueStore(
    const string &name,
    KeyValueStore(CellularCapabilityUniversal::*get)(Error *error),
    void(CellularCapabilityUniversal::*set)(
        const KeyValueStore &value, Error *error)) {
  cellular()->mutable_store()->RegisterDerivedKeyValueStore(
      name,
      KeyValueStoreAccessor(
          new CustomAccessor<CellularCapabilityUniversal, KeyValueStore>(
              this, get, set)));
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

  // ModemManager must be in the disabled state to accept the Enable command.
  Cellular::ModemState state =
      static_cast<Cellular::ModemState>(modem_proxy_->State());
  if (state == Cellular::kModemStateDisabled) {
    EnableModem(error, callback);
  } else if (!cellular()->IsUnderlyingDeviceEnabled()) {
    SLOG(Cellular, 2) << "Enabling of modem deferred because state is "
                      << state;
    deferred_enable_modem_callback_ =
        Bind(&CellularCapabilityUniversal::EnableModem,
             weak_ptr_factory_.GetWeakPtr(), static_cast<Error *>(NULL),
             callback);
  } else {
    // This request cannot be completed synchronously here because a method
    // reply requires a continuation tag that is not created until the message
    // handler returns, see DBus-C++ ObjectAdapter::handle_message().
    cellular()->dispatcher()->PostTask(
        Bind(&CellularCapabilityUniversal::Start_ModemAlreadyEnabled,
             weak_ptr_factory_.GetWeakPtr(), callback));
  }
}

void CellularCapabilityUniversal::EnableModem(Error *error,
                                              const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error local_error(Error::kOperationInitiated);
  modem_info()->metrics()->NotifyDeviceEnableStarted(
      cellular()->interface_index());
  modem_proxy_->Enable(
      true,
      &local_error,
      Bind(&CellularCapabilityUniversal::Start_EnableModemCompleted,
           weak_ptr_factory_.GetWeakPtr(), callback),
      kTimeoutEnable);
  if (local_error.IsFailure()) {
    SLOG(Cellular, 2) << __func__ << "Call to modem_proxy_->Enable() failed";
  }
  if (error) {
    error->CopyFrom(local_error);
  }
}

void CellularCapabilityUniversal::Start_ModemAlreadyEnabled(
    const ResultCallback &callback) {
  // Call GetProperties() here to sync up with the modem state
  GetProperties();
  callback.Run(Error());
}

void CellularCapabilityUniversal::Start_EnableModemCompleted(
    const ResultCallback &callback, const Error &error) {
  SLOG(Cellular, 2) << __func__ << ": " << error;
  if (error.IsFailure()) {
    callback.Run(error);
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
  Cellular::ModemState state = cellular()->modem_state();
  SLOG(Cellular, 2) << __func__ << "(" << state << ")";

  if (cellular()->IsModemRegistered()) {
    string all_bearers("/");  // "/" means all bearers.  See Modemanager docs.
    modem_simple_proxy_->Disconnect(
        all_bearers,
        error,
        Bind(&CellularCapabilityUniversal::Stop_DisconnectCompleted,
             weak_ptr_factory_.GetWeakPtr(), callback),
        kTimeoutDisconnect);
    if (error->IsFailure())
      callback.Run(*error);
  } else {
    Closure task = Bind(&CellularCapabilityUniversal::Stop_Disable,
                        weak_ptr_factory_.GetWeakPtr(),
                        callback);
    cellular()->dispatcher()->PostTask(task);
  }
  deferred_enable_modem_callback_.Reset();
}

void CellularCapabilityUniversal::Stop_DisconnectCompleted(
    const ResultCallback &callback, const Error &error) {
  SLOG(Cellular, 2) << __func__;

  LOG_IF(ERROR, error.IsFailure()) << "Disconnect failed.  Ignoring.";
  Stop_Disable(callback);
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
  if (bearer_path_.empty()) {
    LOG(WARNING) << "In " << __func__ << "(): "
                 << "Ignoring attempt to disconnect without bearer";
  } else if (modem_simple_proxy_.get()) {
    modem_simple_proxy_->Disconnect(bearer_path_,
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
  if (sim_identifier_.empty() ||
      modem_info()->activating_iccid_store()->GetActivationState(
          sim_identifier_) == ActivatingIccidStore::kStateActivated) {
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
  modem_info()->activating_iccid_store()->SetActivationState(
      sim_identifier_,
      ActivatingIccidStore::kStatePendingTimeout);
  ResetAfterActivation();
}

void CellularCapabilityUniversal::CompleteActivation(Error *error) {
  SLOG(Cellular, 2) << __func__;

  // Persist the ICCID as "Pending Activation".
  // We're assuming that when this function gets called, |sim_identifier_| will
  // be non-empty. We still check here that is non-empty, though something is
  // wrong if it is empty.
  if (IsMdnValid()) {
    SLOG(Cellular, 2) << "Already acquired a valid MDN. Already activated.";
    return;
  }

  if (sim_identifier_.empty()) {
    SLOG(Cellular, 2) << "SIM identifier not available. Nothing to do.";
    return;
  }

  // There should be a cellular service at this point.
  if (cellular()->service().get())
    cellular()->service()->SetActivationState(
        flimflam::kActivationStateActivating);
  modem_info()->activating_iccid_store()->SetActivationState(
      sim_identifier_,
      ActivatingIccidStore::kStatePending);

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
  UpdateIccidActivationState();
}

void CellularCapabilityUniversal::UpdateIccidActivationState() {
  SLOG(Cellular, 2) << __func__;

  bool registered =
      registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_HOME;

  // If we have a valid MDN, the service is activated. Always try to remove
  // the ICCID from persistence.
  bool got_mdn = IsMdnValid();
  if (got_mdn && !sim_identifier_.empty())
      modem_info()->activating_iccid_store()->RemoveEntry(sim_identifier_);

  CellularServiceRefPtr service = cellular()->service();

  if (!service.get())
    return;

  if (service->activation_state() == flimflam::kActivationStateActivated)
      // Either no service or already activated. Nothing to do.
      return;

  // If we have a valid MDN or we can connect to the network, then the service
  // is activated.
  if (got_mdn || cellular()->state() == Cellular::kStateConnected ||
      cellular()->state() == Cellular::kStateLinked) {
    SLOG(Cellular, 2) << "Marking service as activated.";
    service->SetActivationState(flimflam::kActivationStateActivated);
    return;
  }

  // If the ICCID is not available, the following logic can be delayed until it
  // becomes available.
  if (sim_identifier_.empty())
    return;

  ActivatingIccidStore::State state =
      modem_info()->activating_iccid_store()->GetActivationState(
          sim_identifier_);
  switch (state) {
    case ActivatingIccidStore::kStatePending:
      // Always mark the service as activating here, as the ICCID could have
      // been unavailable earlier.
      service->SetActivationState(flimflam::kActivationStateActivating);
      if (reset_done_) {
        SLOG(Cellular, 2) << "Post-payment activation reset complete.";
        modem_info()->activating_iccid_store()->SetActivationState(
            sim_identifier_,
            ActivatingIccidStore::kStateActivated);
      } else if (registered) {
        SLOG(Cellular, 2) << "Resetting modem for activation.";
        activation_wait_for_registration_callback_.Cancel();
        ResetAfterActivation();
      }
      break;
    case ActivatingIccidStore::kStateActivated:
      if (registered) {
        // Trigger auto connect here.
        SLOG(Cellular, 2) << "Modem has been reset at least once, try to "
                          << "autoconnect to force MDN to update.";
        service->AutoConnect();
      }
      break;
    case ActivatingIccidStore::kStatePendingTimeout:
      SLOG(Cellular, 2) << "Modem failed to register within timeout, but has "
                        << "been reset at least once.";
      if (registered) {
        SLOG(Cellular, 2) << "Registered to network, marking as activated.";
        modem_info()->activating_iccid_store()->SetActivationState(
            sim_identifier_,
            ActivatingIccidStore::kStateActivated);
        service->SetActivationState(flimflam::kActivationStateActivated);
      }
      break;
    case ActivatingIccidStore::kStateUnknown:
      // No entry exists for this ICCID. Nothing to do.
      break;
    default:
      NOTREACHED();
  }
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
  const string prefix = "cellular_" + cellular()->address() + "_";
  string storage_id;
  if (!operator_id_.empty()) {
    const CellularOperatorInfo::CellularOperator *provider =
        modem_info()->cellular_operator_info()->GetCellularOperatorByMCCMNC(
            operator_id_);
    if (provider && !provider->identifier().empty()) {
      storage_id = prefix + provider->identifier();
    }
  }
  // If the above didn't work, append IMSI, if available.
  if (storage_id.empty() && !imsi_.empty()) {
    storage_id = prefix + imsi_;
  }
  if (!storage_id.empty()) {
    cellular()->service()->SetStorageIdentifier(storage_id);
  }
}

void CellularCapabilityUniversal::UpdateServiceActivationState() {
  if (!cellular()->service().get())
    return;
  bool activation_required = IsServiceActivationRequired();
  string activation_state = flimflam::kActivationStateActivated;
  ActivatingIccidStore::State state =
      modem_info()->activating_iccid_store()->GetActivationState(
          sim_identifier_);
  if (!sim_identifier_.empty() &&
      (state == ActivatingIccidStore::kStatePending ||
       state == ActivatingIccidStore::kStatePendingTimeout))
    activation_state = flimflam::kActivationStateActivating;
  else if (activation_required)
    activation_state = flimflam::kActivationStateNotActivated;
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
  UpdateScanningProperty();
  UpdateServingOperator();
  UpdateOLP();

  // WORKAROUND:
  // E362 modems on Verizon network does not properly redirect when a SIM
  // runs out of credits, we need to enforce out-of-credits detection.
  // TODO(thieule): Remove this workaround (crosbug.com/p/18619).
  if (model_id_ == kE362ModelId)
    cellular()->service()->set_enforce_out_of_credits_detection(true);

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

  apn_try_list_.insert(apn_try_list_.end(), apn_list_.begin(), apn_list_.end());
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
    SLOG(Cellular, 2) << __func__ << ": Using APN "
                      << apn_info[flimflam::kApnProperty];
    (*properties)[kConnectApn].writer().append_string(
        apn_info[flimflam::kApnProperty].c_str());
    if (ContainsKey(apn_info, flimflam::kApnUsernameProperty))
      (*properties)[kConnectUser].writer().append_string(
          apn_info[flimflam::kApnUsernameProperty].c_str());
    if (ContainsKey(apn_info, flimflam::kApnPasswordProperty))
      (*properties)[kConnectPassword].writer().append_string(
          apn_info[flimflam::kApnPasswordProperty].c_str());
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
    bearer_path_ = path;
  }

  if (!callback.is_null())
    callback.Run(error);

  UpdateIccidActivationState();
}

bool CellularCapabilityUniversal::AllowRoaming() {
  return provider_requires_roaming_ || allow_roaming_property();
}

bool CellularCapabilityUniversal::ShouldDetectOutOfCredit() const {
  return model_id_ == kE362ModelId;
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
  SLOG(Cellular, 2) << __func__ << "(IMSI: " << imsi_
          << " SPN: " << spn_ << ")";

  if (!modem_info()->provider_db())
    return;

  // MCCMNC can be determined either from IMSI or Operator Code. Use whichever
  // one is available. If both were reported by the SIM, use IMSI.
  const string &network_id = imsi_.empty() ? operator_id_ : imsi_;
  mobile_provider *provider = mobile_provider_lookup_best_match(
      modem_info()->provider_db(),
      spn_.c_str(),
      network_id.c_str());
  if (!provider) {
    SLOG(Cellular, 2) << "3GPP provider not found.";
    return;
  }

  // Even if provider is the same as home_provider_, it is possible
  // that the spn_ has changed.  Run all the code below.
  home_provider_ = provider;
  provider_requires_roaming_ = home_provider_->requires_roaming;
  Cellular::Operator oper;
  // If Operator ID is available, use that as network code, otherwise
  // use what was returned from the database.
  if (!operator_id_.empty()) {
    oper.SetCode(operator_id_);
  } else if (provider->networks && provider->networks[0]) {
    oper.SetCode(provider->networks[0]);
  }
  if (provider->country) {
    oper.SetCountry(provider->country);
  }
  if (spn_.empty()) {
    const char *name = mobile_provider_get_name(provider);
    if (name) {
      oper.SetName(name);
    }
  } else {
    oper.SetName(spn_);
  }
  cellular()->set_home_provider(oper);
  SLOG(Cellular, 2) << "Home provider: " << oper.GetCode() << ", "
                    << oper.GetName() << ", " << oper.GetCountry()
                    << (provider_requires_roaming_ ? ", roaming required" : "");
  InitAPNList();
  UpdateServiceName();
}

void CellularCapabilityUniversal::UpdateScanningProperty() {
  // Set the Scanning property to true if there is a ongoing network scan
  // (i.e. |scanning_| is true) or the modem is enabled but not yet registered
  // to a network.
  //
  // TODO(benchan): As the Device DBus interface currently does not have a
  // State property to indicate whether the device is being enabled, set the
  // Scanning property to true when the modem is being enabled such that
  // the network UI can start showing the initializing/scanning animation as
  // soon as the modem is being enabled.
  Cellular::ModemState modem_state = cellular()->modem_state();
  bool is_activated_service_waiting_for_registration =
      ((modem_state == Cellular::kModemStateEnabled ||
        modem_state == Cellular::kModemStateSearching) &&
       !IsServiceActivationRequired());
  bool new_scanning_or_searching =
      modem_state == Cellular::kModemStateEnabling ||
      is_activated_service_waiting_for_registration ||
      scanning_;
  if (new_scanning_or_searching != scanning_or_searching_) {
    scanning_or_searching_ = new_scanning_or_searching;
    cellular()->adaptor()->EmitBoolChanged(flimflam::kScanningProperty,
                                           new_scanning_or_searching);

    if (!scanning_or_searching_) {
      SLOG(Cellular, 2) << "Initial network scan ended. Canceling timeout.";
      scanning_or_searching_timeout_callback_.Cancel();
    } else if (scanning_or_searching_timeout_callback_.IsCancelled()) {
      SLOG(Cellular, 2) << "Initial network scan started. Starting timeout.";
      scanning_or_searching_timeout_callback_.Reset(
          Bind(&CellularCapabilityUniversal::OnScanningOrSearchingTimeout,
               weak_ptr_factory_.GetWeakPtr()));
      cellular()->dispatcher()->PostDelayedTask(
          scanning_or_searching_timeout_callback_.callback(),
          scanning_or_searching_timeout_milliseconds_);
    }
  }
}

void CellularCapabilityUniversal::OnScanningOrSearchingTimeout() {
  SLOG(Cellular, 2) << "Initial network scan timed out. Changing "
                    << "flimflam::kScanningProperty to |false|.";
  scanning_or_searching_ = false;
  cellular()->adaptor()->EmitBoolChanged(flimflam::kScanningProperty, false);
}

void CellularCapabilityUniversal::UpdateOLP() {
  if (!modem_info()->cellular_operator_info())
    return;

  const CellularService::OLP *result =
      modem_info()->cellular_operator_info()->GetOLPByMCCMNC(operator_id_);
  if (!result)
    return;

  CellularService::OLP olp;
  olp.CopyFrom(*result);
  string post_data = olp.GetPostData();
  ReplaceSubstringsAfterOffset(&post_data, 0, "${iccid}", sim_identifier_);
  ReplaceSubstringsAfterOffset(&post_data, 0, "${imei}", imei_);
  ReplaceSubstringsAfterOffset(&post_data, 0, "${imsi}", imsi_);
  ReplaceSubstringsAfterOffset(&post_data, 0, "${mdn}", mdn_);
  ReplaceSubstringsAfterOffset(&post_data, 0, "${min}", min_);
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

void CellularCapabilityUniversal::UpdateBearerPath() {
  SLOG(Cellular, 2) << __func__;
  DBusPathsCallback cb = Bind(&CellularCapabilityUniversal::OnListBearersReply,
                              weak_ptr_factory_.GetWeakPtr());
  Error error;
  modem_proxy_->ListBearers(&error, cb, kTimeoutDefault);
}

void CellularCapabilityUniversal::OnListBearersReply(
    const std::vector<DBus::Path> &paths,
    const Error &error) {
  SLOG(Cellular, 2) << __func__ << "(" << error << ")";
  if (error.IsFailure()) {
    SLOG(Cellular, 2) << "ListBearers failed with error: " << error;
    return;
  }
  // Look for the first active bearer and use its path as the connected
  // one. Right now, we don't allow more than one active bearer.
  bool found_active = false;
  for (size_t i = 0; i < paths.size(); ++i) {
    const DBus::Path &path = paths[i];
    scoped_ptr<mm1::BearerProxyInterface> bearer_proxy(
        proxy_factory()->CreateBearerProxy(path, cellular()->dbus_owner()));
    bool is_active = bearer_proxy->Connected();
    if (is_active) {
      if (!found_active) {
        SLOG(Cellular, 2) << "Found active bearer \"" << path << "\".";
        bearer_path_ = path;
        found_active = true;
      } else {
        LOG(FATAL) << "Found more than one active bearer.";
      }
    }
  }
  if (!found_active) {
    SLOG(Cellular, 2) << "No active bearer found, clearing bearer_path_.";
    bearer_path_.clear();
  }
}

void CellularCapabilityUniversal::InitAPNList() {
  SLOG(Cellular, 2) << __func__;
  if (!home_provider_) {
    return;
  }
  apn_list_.clear();
  for (int i = 0; i < home_provider_->num_apns; ++i) {
    Stringmap props;
    mobile_apn *apn = home_provider_->apns[i];
    if (apn->value) {
      props[flimflam::kApnProperty] = apn->value;
    }
    if (apn->username) {
      props[flimflam::kApnUsernameProperty] = apn->username;
    }
    if (apn->password) {
      props[flimflam::kApnPasswordProperty] = apn->password;
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
      props[flimflam::kApnNameProperty] = name->name;
    }
    if (lname) {
      props[flimflam::kApnLocalizedNameProperty] = lname->name;
      props[flimflam::kApnLanguageProperty] = lname->lang;
    }
    apn_list_.push_back(props);
  }
  if (cellular()->adaptor()) {
    cellular()->adaptor()->EmitStringmapsChanged(
        flimflam::kCellularApnListProperty, apn_list_);
  } else {
    LOG(ERROR) << "Null RPC service adaptor.";
  }
}

bool CellularCapabilityUniversal::IsServiceActivationRequired() const {
  if (!sim_identifier_.empty() &&
      modem_info()->activating_iccid_store()->GetActivationState(
          sim_identifier_) != ActivatingIccidStore::kStateUnknown)
    return false;

  // If there is no online payment portal information, it's safer to assume
  // the service does not require activation.
  if (!modem_info()->cellular_operator_info())
    return false;

  const CellularService::OLP *olp =
      modem_info()->cellular_operator_info()->GetOLPByMCCMNC(operator_id_);
  if (!olp)
    return false;

  // To avoid false positives, it's safer to assume the service does not
  // require activation if MDN is not set.
  if (mdn_.empty())
    return false;

  // If MDN contains only zeros, the service requires activation.
  return !IsMdnValid();
}

bool CellularCapabilityUniversal::IsMdnValid() const {
  // Note that |mdn_| is normalized to contain only digits in OnMdnChanged().
  for (size_t i = 0; i < mdn_.size(); ++i) {
    if (mdn_[i] != '0')
      return true;
  }
  return false;
}

// always called from an async context
void CellularCapabilityUniversal::Register(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << " \"" << selected_network_ << "\"";
  CHECK(!callback.is_null());
  Error error;
  ResultCallback cb = Bind(&CellularCapabilityUniversal::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Register(selected_network_, &error, cb, kTimeoutRegister);
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
    selected_network_ = desired_network_;
    desired_network_.clear();
    callback.Run(error);
    return;
  }
  // If registration on the desired network failed,
  // try to register on the home network.
  if (!desired_network_.empty()) {
    desired_network_.clear();
    selected_network_.clear();
    LOG(INFO) << "Couldn't register on selected network, trying home network";
    Register(callback);
    return;
  }
  callback.Run(error);
}

bool CellularCapabilityUniversal::IsRegistered() {
  return (registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_HOME ||
          registration_state_ == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING);
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
  sim_proxy_->SendPin(pin, error, callback, kTimeoutDefault);
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

void CellularCapabilityUniversal::Scan(Error *error,
                                       const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(error);
  if (scanning_) {
    Error::PopulateAndLog(error, Error::kInProgress, "Already scanning");
    return;
  }
  DBusPropertyMapsCallback cb = Bind(&CellularCapabilityUniversal::OnScanReply,
                                     weak_ptr_factory_.GetWeakPtr(), callback);
  modem_3gpp_proxy_->Scan(error, cb, kTimeoutScan);
  if (!error->IsFailure()) {
    scanning_ = true;
    UpdateScanningProperty();
  }
}

void CellularCapabilityUniversal::OnScanReply(const ResultCallback &callback,
                                              const ScanResults &results,
                                              const Error &error) {
  SLOG(Cellular, 2) << __func__;

  // Error handling is weak.  The current expectation is that on any
  // error, found_networks_ should be cleared and a property change
  // notification sent out.
  //
  // TODO(jglasgow): fix error handling
  scanning_ = false;
  UpdateScanningProperty();
  found_networks_.clear();
  if (!error.IsFailure()) {
    for (ScanResults::const_iterator it = results.begin();
         it != results.end(); ++it) {
      found_networks_.push_back(ParseScanResult(*it));
    }
  }
  cellular()->adaptor()->EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                               found_networks_);

  // TODO(gmorain): This check for is_null() is a work-around because
  // Cellular::Scan() passes a null callback.  Instead: 1. Have Cellular::Scan()
  // pass in a callback. 2. Have Cellular "own" the found_networks_ property
  // 3. Have Cellular EmitStingMapsChanged() 4. Share the code between GSM and
  // Universal.
  if (!callback.is_null())
    callback.Run(error);
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
    parsed[flimflam::kStatusProperty] = kStatusString[status];
  }

  uint32 tech;  // MMModemAccessTechnology
  if (DBusProperties::GetUint32(result, kOperatorAccessTechnologyProperty,
                                &tech)) {
    parsed[flimflam::kTechnologyProperty] = AccessTechnologyToString(tech);
  }

  string operator_long, operator_short, operator_code;
  if (DBusProperties::GetString(result, kOperatorLongProperty, &operator_long))
    parsed[flimflam::kLongNameProperty] = operator_long;
  if (DBusProperties::GetString(result, kOperatorShortProperty,
                                &operator_short))
    parsed[flimflam::kShortNameProperty] = operator_short;
  if (DBusProperties::GetString(result, kOperatorCodeProperty, &operator_code))
    parsed[flimflam::kNetworkIdProperty] = operator_code;

  // If the long name is not available but the network ID is, look up the long
  // name in the mobile provider database.
  if ((!ContainsKey(parsed, flimflam::kLongNameProperty) ||
       parsed[flimflam::kLongNameProperty].empty()) &&
      ContainsKey(parsed, flimflam::kNetworkIdProperty)) {
    mobile_provider *provider =
        mobile_provider_lookup_by_network(
            modem_info()->provider_db(),
            parsed[flimflam::kNetworkIdProperty].c_str());
    if (provider) {
      const char *long_name = mobile_provider_get_name(provider);
      if (long_name && *long_name) {
        parsed[flimflam::kLongNameProperty] = long_name;
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
  if (model_id_ == kE362ModelId)
    return flimflam::kNetworkTechnologyLte;

  // Order is important.  Return the highest speed technology
  // TODO(jglasgow): change shill interfaces to a capability model
  return AccessTechnologyToString(access_technologies_);
}

string CellularCapabilityUniversal::GetRoamingStateString() const {
  switch (registration_state_) {
    case MM_MODEM_3GPP_REGISTRATION_STATE_HOME:
      return flimflam::kRoamingStateHome;
    case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING:
      return flimflam::kRoamingStateRoaming;
    default:
      break;
  }
  return flimflam::kRoamingStateUnknown;
}

void CellularCapabilityUniversal::GetSignalQuality() {
  // TODO(njw): Switch to asynchronous calls (crosbug.com/17583).
  const DBus::Struct<unsigned int, bool> quality =
      modem_proxy_->SignalQuality();
  OnSignalQualityChanged(quality._1);
}

string CellularCapabilityUniversal::GetTypeString() const {
  return AccessTechnologyToTechnologyFamily(access_technologies_);
}

void CellularCapabilityUniversal::OnModemPropertiesChanged(
    const DBusPropertiesMap &properties,
    const vector<string> &/* invalidated_properties */) {
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
  string string_value;
  if (DBusProperties::GetObjectPath(properties,
                                    MM_MODEM_PROPERTY_SIM, &string_value))
    OnSimPathChanged(string_value);
  uint32 uint_value;
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_MODEMCAPABILITIES,
                                &uint_value))
    OnModemCapabilitesChanged(uint_value);
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_CURRENTCAPABILITIES,
                                &uint_value))
    OnModemCurrentCapabilitiesChanged(uint_value);
  // not needed: MM_MODEM_PROPERTY_MAXBEARERS
  // not needed: MM_MODEM_PROPERTY_MAXACTIVEBEARERS
  if (DBusProperties::GetString(properties,
                                MM_MODEM_PROPERTY_MANUFACTURER,
                                &string_value))
    OnModemManufacturerChanged(string_value);
  if (DBusProperties::GetString(properties,
                                MM_MODEM_PROPERTY_MODEL,
                                &string_value))
    OnModemModelChanged(string_value);
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
  bool locks_changed = false;
  uint32_t unlock_required;  // This is really of type MMModemLock
  if (DBusProperties::GetUint32(properties,
                                MM_MODEM_PROPERTY_UNLOCKREQUIRED,
                                &unlock_required)) {
    locks_changed = true;
  }
  LockRetryData lock_retries;
  DBusPropertiesMap::const_iterator it =
      properties.find(MM_MODEM_PROPERTY_UNLOCKRETRIES);
  if (it != properties.end()) {
    lock_retries = it->second.operator LockRetryData();
    locks_changed = true;
  }
  if (locks_changed)
    OnLockRetriesChanged(static_cast<MMModemLock>(unlock_required),
                         lock_retries);
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
  if (DBusProperties::GetUint32(properties, MM_MODEM_PROPERTY_SUPPORTEDMODES,
                                &uint_value))
    OnSupportedModesChanged(uint_value);
  if (DBusProperties::GetUint32(properties, MM_MODEM_PROPERTY_ALLOWEDMODES,
                                &uint_value))
    OnAllowedModesChanged(uint_value);
  if (DBusProperties::GetUint32(properties, MM_MODEM_PROPERTY_PREFERREDMODE,
                                &uint_value))
    OnPreferredModeChanged(static_cast<MMModemMode>(uint_value));
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
  if ((model_id_ == kE362ModelId) && (error.type() == Error::kOperationFailed))
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
    imsi_ = "";
    spn_ = "";
    sim_present_ = false;
    OnSimIdentifierChanged("");
    OnOperatorIdChanged("");
  } else {
    sim_present_ = true;
    scoped_ptr<DBusPropertiesProxyInterface> properties_proxy(
        proxy_factory()->CreateDBusPropertiesProxy(sim_path,
                                                   cellular()->dbus_owner()));
    // TODO(jglasgow): convert to async interface
    DBusPropertiesMap properties(
        properties_proxy->GetAll(MM_DBUS_INTERFACE_SIM));
    OnSimPropertiesChanged(properties, vector<string>());
  }
}

void CellularCapabilityUniversal::OnModemCapabilitesChanged(
    uint32 capabilities) {
  capabilities_ = capabilities;
}

void CellularCapabilityUniversal::OnModemCurrentCapabilitiesChanged(
    uint32 current_capabilities) {
  current_capabilities_ = current_capabilities;

  // Only allow network scan when the modem's current capabilities support
  // GSM/UMTS.
  //
  // TODO(benchan): We should consider having the modem plugins in ModemManager
  // reporting whether network scan is supported.
  scanning_supported_ =
      (current_capabilities & MM_MODEM_CAPABILITY_GSM_UMTS) != 0;
  if (cellular()->adaptor()) {
    cellular()->adaptor()->EmitBoolChanged(
        flimflam::kSupportNetworkScanProperty, scanning_supported_);
  }
}

void CellularCapabilityUniversal::OnMdnChanged(
    const string &mdn) {
  mdn_ = NormalizeMdn(mdn);
  UpdateIccidActivationState();
}

void CellularCapabilityUniversal::OnModemManufacturerChanged(
    const string &manufacturer) {
  manufacturer_ = manufacturer;
}

void CellularCapabilityUniversal::OnModemModelChanged(
    const string &model) {
  model_id_ = model;
}

void CellularCapabilityUniversal::OnModemRevisionChanged(
    const string &revision) {
  firmware_revision_ = revision;
}

void CellularCapabilityUniversal::OnModemStateChanged(
    Cellular::ModemState state) {
  Cellular::ModemState prev_modem_state = cellular()->modem_state();
  bool was_enabled = cellular()->IsUnderlyingDeviceEnabled();
  if (Cellular::IsEnabledModemState(state))
    cellular()->set_modem_state(state);
  SLOG(Cellular, 2) << __func__ << ": prev_modem_state: " << prev_modem_state
                    << " was_enabled: " << was_enabled
                    << " cellular state: "
                    << cellular()->GetStateString(cellular()->state());
  if (prev_modem_state != Cellular::kModemStateUnknown &&
      prev_modem_state != Cellular::kModemStateEnabling &&
      !was_enabled &&
      cellular()->state() == Cellular::kStateDisabled &&
      cellular()->IsUnderlyingDeviceEnabled()) {
    cellular()->SetEnabled(true);
  }
  UpdateScanningProperty();
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
          flimflam::kTechnologyFamilyProperty, new_type_string);
    }
    if (cellular()->service().get()) {
      cellular()->service()->SetNetworkTechnology(GetNetworkTechnologyString());
    }
  }
}

void CellularCapabilityUniversal::OnSupportedModesChanged(
    uint32 supported_modes) {
  supported_modes_ = supported_modes;
}

void CellularCapabilityUniversal::OnAllowedModesChanged(
    uint32 allowed_modes) {
  allowed_modes_ = allowed_modes;
}

void CellularCapabilityUniversal::OnPreferredModeChanged(
    MMModemMode preferred_mode) {
  preferred_mode_ = preferred_mode;
}

void CellularCapabilityUniversal::OnLockRetriesChanged(
    MMModemLock unlock_required,
    const LockRetryData &lock_retries) {
  switch (unlock_required) {
    case MM_MODEM_LOCK_SIM_PIN:
      sim_lock_status_.lock_type = "sim-pin";
      break;
    case MM_MODEM_LOCK_SIM_PUK:
      sim_lock_status_.lock_type = "sim-puk";
      break;
    default:
      sim_lock_status_.lock_type = "";
      break;
  }
  LockRetryData::const_iterator it = lock_retries.find(unlock_required);
  if (it != lock_retries.end()) {
    sim_lock_status_.retries_left = it->second;
  } else {
    // Unknown, use 999
    sim_lock_status_.retries_left = 999;
  }
  OnSimLockStatusChanged();
}

void CellularCapabilityUniversal::OnSimLockStatusChanged() {
  cellular()->adaptor()->EmitKeyValueStoreChanged(
      flimflam::kSIMLockStatusProperty, SimLockStatusToProperty(NULL));

  // If the SIM is currently unlocked, assume that we need to refresh
  // carrier information, since a locked SIM prevents shill from obtaining
  // the necessary data to establish a connection later (e.g. IMSI).
  if (!sim_path_.empty() && sim_lock_status_.lock_type.empty()) {
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
  string imei;
  if (DBusProperties::GetString(properties,
                                MM_MODEM_MODEM3GPP_PROPERTY_IMEI,
                                &imei))
    OnImeiChanged(imei);

  // Handle registration state changes as a single change
  string operator_code = serving_operator_.GetCode();
  string operator_name = serving_operator_.GetName();
  MMModem3gppRegistrationState state = registration_state_;
  bool registration_changed = false;
  uint32 uint_value;
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

  uint32 locks = 0;
  if (DBusProperties::GetUint32(
          properties, MM_MODEM_MODEM3GPP_PROPERTY_ENABLEDFACILITYLOCKS,
          &locks))
    OnFacilityLocksChanged(locks);
}

void CellularCapabilityUniversal::OnImeiChanged(const string &imei) {
  imei_ = imei;
}

void CellularCapabilityUniversal::On3GPPRegistrationChanged(
    MMModem3gppRegistrationState state,
    const string &operator_code,
    const string &operator_name) {
  SLOG(Cellular, 2) << __func__ << ": regstate=" << state
                    << ", opercode=" << operator_code
                    << ", opername=" << operator_name;
  registration_state_ = state;
  serving_operator_.SetCode(operator_code);
  serving_operator_.SetName(operator_name);

  // Update the carrier name for |serving_operator_|.
  UpdateOperatorInfo();

  cellular()->HandleNewRegistrationState();

  // Update the user facing name of the cellular service.
  UpdateServiceName();

  // If the modem registered with the network and the current ICCID is pending
  // activation, then reset the modem.
  UpdateIccidActivationState();
}

void CellularCapabilityUniversal::OnModemStateChangedSignal(
    int32 old_state, int32 new_state, uint32 reason) {
  SLOG(Cellular, 2) << __func__ << "(" << old_state << ", " << new_state << ", "
                    << reason << ")";
  cellular()->OnModemStateChanged(static_cast<Cellular::ModemState>(old_state),
                                  static_cast<Cellular::ModemState>(new_state),
                                  reason);
  UpdateScanningProperty();
  if (!deferred_enable_modem_callback_.is_null() &&
      (new_state == Cellular::kModemStateDisabled)) {
    SLOG(Cellular, 2) << "Enabling modem after deferring";
    deferred_enable_modem_callback_.Run();
    deferred_enable_modem_callback_.Reset();
  } else if (new_state == Cellular::kModemStateConnected) {
    SLOG(Cellular, 2) << "Updating bearer path to reflect the active bearer.";
    UpdateBearerPath();
  }
}

void CellularCapabilityUniversal::OnSignalQualityChanged(uint32 quality) {
  cellular()->HandleNewSignalQuality(quality);
}

void CellularCapabilityUniversal::OnFacilityLocksChanged(uint32 locks) {
  if (sim_lock_status_.enabled != (locks & MM_MODEM_3GPP_FACILITY_SIM)) {
    sim_lock_status_.enabled = locks & MM_MODEM_3GPP_FACILITY_SIM;
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
    OnImsiChanged(value);
  SetHomeProvider();
}

// TODO(armansito): The following methods should probably log their argument
// values. Need to learn if any of them need to be scrubbed.
void CellularCapabilityUniversal::OnImsiChanged(const std::string &imsi) {
  imsi_ = imsi;
}

void CellularCapabilityUniversal::OnSpnChanged(const std::string &spn) {
  spn_ = spn;
}

void CellularCapabilityUniversal::OnSimIdentifierChanged(const string &id) {
  sim_identifier_ = id;
  UpdateIccidActivationState();
}

void CellularCapabilityUniversal::OnOperatorIdChanged(
    const string &operator_id) {
  SLOG(Cellular, 2) << "Operator ID = '" << operator_id << "'";
  operator_id_ = operator_id;
}

}  // namespace shill
