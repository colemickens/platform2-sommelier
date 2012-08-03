// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"

using base::Bind;
using std::string;
using std::vector;

namespace shill {

// static
unsigned int CellularCapabilityGSM::friendly_service_name_id_ = 0;

const char CellularCapabilityGSM::kNetworkPropertyAccessTechnology[] =
    "access-tech";
const char CellularCapabilityGSM::kNetworkPropertyID[] = "operator-num";
const char CellularCapabilityGSM::kNetworkPropertyLongName[] = "operator-long";
const char CellularCapabilityGSM::kNetworkPropertyShortName[] =
    "operator-short";
const char CellularCapabilityGSM::kNetworkPropertyStatus[] = "status";
const char CellularCapabilityGSM::kPhoneNumber[] = "*99#";
const char CellularCapabilityGSM::kPropertyAccessTechnology[] =
    "AccessTechnology";
const char CellularCapabilityGSM::kPropertyEnabledFacilityLocks[] =
    "EnabledFacilityLocks";
const char CellularCapabilityGSM::kPropertyUnlockRequired[] = "UnlockRequired";
const char CellularCapabilityGSM::kPropertyUnlockRetries[] = "UnlockRetries";

const int CellularCapabilityGSM::kGetIMSIRetryLimit = 10;
const int64 CellularCapabilityGSM::kGetIMSIRetryDelayMilliseconds = 200;


CellularCapabilityGSM::CellularCapabilityGSM(Cellular *cellular,
                                             ProxyFactory *proxy_factory)
    : CellularCapabilityClassic(cellular, proxy_factory),
      weak_ptr_factory_(this),
      registration_state_(MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN),
      access_technology_(MM_MODEM_GSM_ACCESS_TECH_UNKNOWN),
      home_provider_(NULL),
      get_imsi_retries_(0),
      get_imsi_retry_delay_milliseconds_(kGetIMSIRetryDelayMilliseconds),
      scanning_(false),
      scan_interval_(0) {
  SLOG(Cellular, 2) << "Cellular capability constructed: GSM";
  PropertyStore *store = cellular->mutable_store();
  store->RegisterConstString(flimflam::kSelectedNetworkProperty,
                             &selected_network_);
  store->RegisterConstStringmaps(flimflam::kFoundNetworksProperty,
                                 &found_networks_);
  store->RegisterConstBool(flimflam::kScanningProperty, &scanning_);
  store->RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);
  HelpRegisterDerivedKeyValueStore(
      flimflam::kSIMLockStatusProperty,
      &CellularCapabilityGSM::SimLockStatusToProperty,
      NULL);
  store->RegisterConstStringmaps(flimflam::kCellularApnListProperty,
                                 &apn_list_);
  scanning_supported_ = true;
}

KeyValueStore CellularCapabilityGSM::SimLockStatusToProperty(Error */*error*/) {
  KeyValueStore status;
  status.SetBool(flimflam::kSIMLockEnabledProperty, sim_lock_status_.enabled);
  status.SetString(flimflam::kSIMLockTypeProperty, sim_lock_status_.lock_type);
  status.SetUint(flimflam::kSIMLockRetriesLeftProperty,
                 sim_lock_status_.retries_left);
  return status;
}

void CellularCapabilityGSM::HelpRegisterDerivedKeyValueStore(
    const string &name,
    KeyValueStore(CellularCapabilityGSM::*get)(Error *error),
    void(CellularCapabilityGSM::*set)(
        const KeyValueStore &value, Error *error)) {
  cellular()->mutable_store()->RegisterDerivedKeyValueStore(
      name,
      KeyValueStoreAccessor(
          new CustomAccessor<CellularCapabilityGSM, KeyValueStore>(
              this, get, set)));
}

void CellularCapabilityGSM::InitProxies() {
  CellularCapabilityClassic::InitProxies();
  card_proxy_.reset(
      proxy_factory()->CreateModemGSMCardProxy(cellular()->dbus_path(),
                                               cellular()->dbus_owner()));
  network_proxy_.reset(
      proxy_factory()->CreateModemGSMNetworkProxy(cellular()->dbus_path(),
                                                  cellular()->dbus_owner()));
  network_proxy_->set_signal_quality_callback(
      Bind(&CellularCapabilityGSM::OnSignalQualitySignal,
           weak_ptr_factory_.GetWeakPtr()));
  network_proxy_->set_network_mode_callback(
      Bind(&CellularCapabilityGSM::OnNetworkModeSignal,
           weak_ptr_factory_.GetWeakPtr()));
  network_proxy_->set_registration_info_callback(
      Bind(&CellularCapabilityGSM::OnRegistrationInfoSignal,
           weak_ptr_factory_.GetWeakPtr()));
}

void CellularCapabilityGSM::StartModem(Error *error,
                                       const ResultCallback &callback) {
  InitProxies();

  CellularTaskList *tasks = new CellularTaskList();
  ResultCallback cb =
      Bind(&CellularCapabilityGSM::StepCompletedCallback,
           weak_ptr_factory_.GetWeakPtr(), callback, false, tasks);
  ResultCallback cb_ignore_error =
        Bind(&CellularCapabilityGSM::StepCompletedCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback, true, tasks);
  if (!cellular()->IsUnderlyingDeviceEnabled())
    tasks->push_back(Bind(&CellularCapabilityGSM::EnableModem,
                          weak_ptr_factory_.GetWeakPtr(), cb));
  // If we're within range of the home network, the modem will try to
  // register once it's enabled, or may be already registered if we
  // started out enabled.
  if (!IsUnderlyingDeviceRegistered() && !selected_network_.empty())
    tasks->push_back(Bind(&CellularCapabilityGSM::Register,
                          weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityGSM::GetIMEI,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  get_imsi_retries_ = 0;
  tasks->push_back(Bind(&CellularCapabilityGSM::GetIMSI,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityGSM::GetSPN,
                        weak_ptr_factory_.GetWeakPtr(), cb_ignore_error));
  tasks->push_back(Bind(&CellularCapabilityGSM::GetMSISDN,
                        weak_ptr_factory_.GetWeakPtr(), cb_ignore_error));
  tasks->push_back(Bind(&CellularCapabilityGSM::GetProperties,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityGSM::GetModemInfo,
                        weak_ptr_factory_.GetWeakPtr(), cb_ignore_error));
  tasks->push_back(Bind(&CellularCapabilityGSM::FinishEnable,
                        weak_ptr_factory_.GetWeakPtr(), cb));

  RunNextStep(tasks);
}

bool CellularCapabilityGSM::IsUnderlyingDeviceRegistered() const {
  switch (cellular()->modem_state()) {
    case Cellular::kModemStateUnknown:
    case Cellular::kModemStateDisabled:
    case Cellular::kModemStateInitializing:
    case Cellular::kModemStateLocked:
    case Cellular::kModemStateDisabling:
    case Cellular::kModemStateEnabling:
    case Cellular::kModemStateEnabled:
      return false;
    case Cellular::kModemStateSearching:
    case Cellular::kModemStateRegistered:
    case Cellular::kModemStateDisconnecting:
    case Cellular::kModemStateConnecting:
    case Cellular::kModemStateConnected:
      return true;
  }
  return false;
}

void CellularCapabilityGSM::ReleaseProxies() {
  SLOG(Cellular, 2) << __func__;
  CellularCapabilityClassic::ReleaseProxies();
  card_proxy_.reset();
  network_proxy_.reset();
}

void CellularCapabilityGSM::OnServiceCreated() {
  // If IMSI is available, base the service's storage identifier on it.
  if (!imsi_.empty()) {
    cellular()->service()->SetStorageIdentifier(
        string(flimflam::kTypeCellular) + "_" +
        cellular()->address() + "_" + imsi_);
  }
  cellular()->service()->SetActivationState(
      flimflam::kActivationStateActivated);
  UpdateServingOperator();
}

void CellularCapabilityGSM::UpdateStatus(const DBusPropertiesMap &properties) {
  if (ContainsKey(properties, kModemPropertyIMSI)) {
    SetHomeProvider();
  }
}

// Create the list of APNs to try, in the following order:
// - last APN that resulted in a successful connection attempt on the
//   current network (if any)
// - the APN, if any, that was set by the user
// - the list of APNs found in the mobile broadband provider DB for the
//   home provider associated with the current SIM
// - as a last resort, attempt to connect with no APN
void CellularCapabilityGSM::SetupApnTryList() {
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

void CellularCapabilityGSM::SetupConnectProperties(
    DBusPropertiesMap *properties) {
  SetupApnTryList();
  FillConnectPropertyMap(properties);
}

void CellularCapabilityGSM::FillConnectPropertyMap(
    DBusPropertiesMap *properties) {
  (*properties)[kConnectPropertyPhoneNumber].writer().append_string(
      kPhoneNumber);

  if (!AllowRoaming())
    (*properties)[kConnectPropertyHomeOnly].writer().append_bool(true);

  if (!apn_try_list_.empty()) {
    // Leave the APN at the front of the list, so that it can be recorded
    // if the connect attempt succeeds.
    Stringmap apn_info = apn_try_list_.front();
    SLOG(Cellular, 2) << __func__ << ": Using APN "
                      << apn_info[flimflam::kApnProperty];
    (*properties)[kConnectPropertyApn].writer().append_string(
        apn_info[flimflam::kApnProperty].c_str());
    if (ContainsKey(apn_info, flimflam::kApnUsernameProperty))
      (*properties)[kConnectPropertyApnUsername].writer().append_string(
          apn_info[flimflam::kApnUsernameProperty].c_str());
    if (ContainsKey(apn_info, flimflam::kApnPasswordProperty))
      (*properties)[kConnectPropertyApnPassword].writer().append_string(
          apn_info[flimflam::kApnPasswordProperty].c_str());
  }
}

void CellularCapabilityGSM::Connect(const DBusPropertiesMap &properties,
                                    Error *error,
                                    const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  ResultCallback cb = Bind(&CellularCapabilityGSM::OnConnectReply,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  simple_proxy_->Connect(properties, error, cb, kTimeoutConnect);
}

void CellularCapabilityGSM::OnConnectReply(const ResultCallback &callback,
                                           const Error &error) {
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
    if (error.type() == Error::kInvalidApn && !apn_try_list_.empty()) {
      apn_try_list_.pop_front();
      SLOG(Cellular, 2) << "Connect failed with invalid APN, "
                        << apn_try_list_.size() << " remaining APNs to try";
      DBusPropertiesMap props;
      FillConnectPropertyMap(&props);
      Error error;
      Connect(props, &error, callback);
      return;
    }
  } else if (!apn_try_list_.empty()) {
    service->SetLastGoodApn(apn_try_list_.front());
    apn_try_list_.clear();
  }
  if (!callback.is_null())
    callback.Run(error);
}

bool CellularCapabilityGSM::AllowRoaming() {
  bool requires_roaming =
      home_provider_ ? home_provider_->requires_roaming : false;
  return requires_roaming || allow_roaming_property();
}

// always called from an async context
void CellularCapabilityGSM::GetIMEI(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  if (imei_.empty()) {
    GSMIdentifierCallback cb = Bind(&CellularCapabilityGSM::OnGetIMEIReply,
                                    weak_ptr_factory_.GetWeakPtr(), callback);
    card_proxy_->GetIMEI(&error, cb, kTimeoutDefault);
    if (error.IsFailure())
      callback.Run(error);
  } else {
    SLOG(Cellular, 2) << "Already have IMEI " << imei_;
    callback.Run(error);
  }
}

// always called from an async context
void CellularCapabilityGSM::GetIMSI(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  if (imsi_.empty()) {
    GSMIdentifierCallback cb = Bind(&CellularCapabilityGSM::OnGetIMSIReply,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback);
    card_proxy_->GetIMSI(&error, cb, kTimeoutDefault);
    if (error.IsFailure())
      callback.Run(error);
  } else {
    SLOG(Cellular, 2) << "Already have IMSI " << imsi_;
    callback.Run(error);
  }
}

// always called from an async context
void CellularCapabilityGSM::GetSPN(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  if (spn_.empty()) {
    GSMIdentifierCallback cb = Bind(&CellularCapabilityGSM::OnGetSPNReply,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback);
    card_proxy_->GetSPN(&error, cb, kTimeoutDefault);
    if (error.IsFailure())
      callback.Run(error);
  } else {
    SLOG(Cellular, 2) << "Already have SPN " << spn_;
    callback.Run(error);
  }
}

// always called from an async context
void CellularCapabilityGSM::GetMSISDN(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  if (mdn_.empty()) {
    GSMIdentifierCallback cb = Bind(&CellularCapabilityGSM::OnGetMSISDNReply,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback);
    card_proxy_->GetMSISDN(&error, cb, kTimeoutDefault);
    if (error.IsFailure())
      callback.Run(error);
  } else {
    SLOG(Cellular, 2) << "Already have MSISDN " << mdn_;
    callback.Run(error);
  }
}

void CellularCapabilityGSM::GetSignalQuality() {
  SLOG(Cellular, 2) << __func__;
  SignalQualityCallback callback =
      Bind(&CellularCapabilityGSM::OnGetSignalQualityReply,
           weak_ptr_factory_.GetWeakPtr());
  network_proxy_->GetSignalQuality(NULL, callback, kTimeoutDefault);
}

void CellularCapabilityGSM::GetRegistrationState() {
  SLOG(Cellular, 2) << __func__;
  RegistrationInfoCallback callback =
      Bind(&CellularCapabilityGSM::OnGetRegistrationInfoReply,
           weak_ptr_factory_.GetWeakPtr());
  network_proxy_->GetRegistrationInfo(NULL, callback, kTimeoutDefault);
}

void CellularCapabilityGSM::GetProperties(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;

  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 tech = network_proxy_->AccessTechnology();
  SetAccessTechnology(tech);
  SLOG(Cellular, 2) << "GSM AccessTechnology: " << tech;

  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 locks = card_proxy_->EnabledFacilityLocks();
  sim_lock_status_.enabled = locks & MM_MODEM_GSM_FACILITY_SIM;
  SLOG(Cellular, 2) << "GSM EnabledFacilityLocks: " << locks;

  callback.Run(Error());
}

string CellularCapabilityGSM::CreateFriendlyServiceName() {
  SLOG(Cellular, 2) << __func__;
  if (registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME &&
      !cellular()->home_provider().GetName().empty()) {
    return cellular()->home_provider().GetName();
  }
  if (!serving_operator_.GetName().empty()) {
    return serving_operator_.GetName();
  }
  if (!carrier_.empty()) {
    return carrier_;
  }
  if (!serving_operator_.GetCode().empty()) {
    return "cellular_" + serving_operator_.GetCode();
  }
  return base::StringPrintf("GSMNetwork%u", friendly_service_name_id_++);
}

void CellularCapabilityGSM::SetHomeProvider() {
  SLOG(Cellular, 2) << __func__ << "(IMSI: " << imsi_
                    << " SPN: " << spn_ << ")";
  // TODO(petkov): The test for NULL provider_db should be done by
  // mobile_provider_lookup_best_match.
  if (imsi_.empty() || !cellular()->provider_db()) {
    return;
  }
  mobile_provider *provider =
      mobile_provider_lookup_best_match(
          cellular()->provider_db(), spn_.c_str(), imsi_.c_str());
  if (!provider) {
    SLOG(Cellular, 2) << "GSM provider not found.";
    return;
  }
  home_provider_ = provider;
  Cellular::Operator oper;
  if (provider->networks) {
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
  InitAPNList();
}

void CellularCapabilityGSM::UpdateOperatorInfo() {
  SLOG(Cellular, 2) << __func__;
  const string &network_id = serving_operator_.GetCode();
  if (!network_id.empty()) {
    SLOG(Cellular, 2) << "Looking up network id: " << network_id;
    mobile_provider *provider =
        mobile_provider_lookup_by_network(cellular()->provider_db(),
                                          network_id.c_str());
    if (provider) {
      const char *provider_name = mobile_provider_get_name(provider);
      if (provider_name && *provider_name) {
        serving_operator_.SetName(provider_name);
        if (provider->country) {
          serving_operator_.SetCountry(provider->country);
        }
        SLOG(Cellular, 2) << "Operator name: " << serving_operator_.GetName()
                          << ", country: " << serving_operator_.GetCountry();
      }
    } else {
      SLOG(Cellular, 2) << "GSM provider not found.";
    }
  }
  UpdateServingOperator();
}

void CellularCapabilityGSM::UpdateServingOperator() {
  SLOG(Cellular, 2) << __func__;
  if (cellular()->service().get()) {
    cellular()->service()->SetServingOperator(serving_operator_);
  }
}

void CellularCapabilityGSM::InitAPNList() {
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
  cellular()->adaptor()->EmitStringmapsChanged(
      flimflam::kCellularApnListProperty, apn_list_);
}

// always called from an async context
void CellularCapabilityGSM::Register(const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << " \"" << selected_network_ << "\"";
  CHECK(!callback.is_null());
  Error error;
  ResultCallback cb = Bind(&CellularCapabilityGSM::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  network_proxy_->Register(selected_network_, &error, cb, kTimeoutRegister);
  if (error.IsFailure())
    callback.Run(error);
}

void CellularCapabilityGSM::RegisterOnNetwork(
    const string &network_id,
    Error *error,
    const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__ << "(" << network_id << ")";
  CHECK(error);
  desired_network_ = network_id;
  ResultCallback cb = Bind(&CellularCapabilityGSM::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  network_proxy_->Register(network_id, error, cb, kTimeoutRegister);
}

void CellularCapabilityGSM::OnRegisterReply(const ResultCallback &callback,
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

bool CellularCapabilityGSM::IsRegistered() {
  return (registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME ||
          registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
}

void CellularCapabilityGSM::SetUnregistered(bool searching) {
  // If we're already in some non-registered state, don't override that
  if (registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME ||
      registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING) {
    registration_state_ =
        (searching ? MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING :
                     MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE);
  }
}

void CellularCapabilityGSM::RequirePIN(
    const std::string &pin, bool require,
    Error *error, const ResultCallback &callback) {
  CHECK(error);
  card_proxy_->EnablePIN(pin, require, error, callback, kTimeoutDefault);
}

void CellularCapabilityGSM::EnterPIN(const string &pin,
                                     Error *error,
                                     const ResultCallback &callback) {
  CHECK(error);
  card_proxy_->SendPIN(pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityGSM::UnblockPIN(const string &unblock_code,
                                       const string &pin,
                                       Error *error,
                                       const ResultCallback &callback) {
  CHECK(error);
  card_proxy_->SendPUK(unblock_code, pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityGSM::ChangePIN(
    const string &old_pin, const string &new_pin,
    Error *error, const ResultCallback &callback) {
  CHECK(error);
  card_proxy_->ChangePIN(old_pin, new_pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityGSM::Scan(Error *error, const ResultCallback &callback) {
  SLOG(Cellular, 2) << __func__;
  CHECK(error);
  if (scanning_) {
    Error::PopulateAndLog(error, Error::kInProgress, "Already scanning");
    return;
  }
  ScanResultsCallback cb = Bind(&CellularCapabilityGSM::OnScanReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  network_proxy_->Scan(error, cb, kTimeoutScan);
  if (!error->IsFailure()) {
    scanning_ = true;
    cellular()->adaptor()->EmitBoolChanged(flimflam::kScanningProperty,
                                           scanning_);
  }
}

void CellularCapabilityGSM::OnScanReply(const ResultCallback &callback,
                                        const GSMScanResults &results,
                                        const Error &error) {
  SLOG(Cellular, 2) << __func__;

  // Error handling is weak.  The current expectation is that on any
  // error, found_networks_ should be cleared and a property change
  // notification sent out.
  //
  // TODO(jglasgow): fix error handling
  scanning_ = false;
  cellular()->adaptor()->EmitBoolChanged(flimflam::kScanningProperty,
                                         scanning_);
  found_networks_.clear();
  if (!error.IsFailure()) {
    for (GSMScanResults::const_iterator it = results.begin();
         it != results.end(); ++it) {
      found_networks_.push_back(ParseScanResult(*it));
    }
  }
  cellular()->adaptor()->EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                               found_networks_);
  if (!callback.is_null())
    callback.Run(error);
}

Stringmap CellularCapabilityGSM::ParseScanResult(const GSMScanResult &result) {
  Stringmap parsed;
  for (GSMScanResult::const_iterator it = result.begin();
       it != result.end(); ++it) {
    // TODO(petkov): Define these in system_api/service_constants.h. The
    // numerical values are taken from 3GPP TS 27.007 Section 7.3.
    static const char * const kStatusString[] = {
      "unknown",
      "available",
      "current",
      "forbidden",
    };
    static const char * const kTechnologyString[] = {
      flimflam::kNetworkTechnologyGsm,
      "GSM Compact",
      flimflam::kNetworkTechnologyUmts,
      flimflam::kNetworkTechnologyEdge,
      "HSDPA",
      "HSUPA",
      flimflam::kNetworkTechnologyHspa,
    };
    SLOG(Cellular, 2) << "Network property: " << it->first << " = "
                      << it->second;
    if (it->first == kNetworkPropertyStatus) {
      int status = 0;
      if (base::StringToInt(it->second, &status) &&
          status >= 0 &&
          status < static_cast<int>(arraysize(kStatusString))) {
        parsed[flimflam::kStatusProperty] = kStatusString[status];
      } else {
        LOG(ERROR) << "Unexpected status value: " << it->second;
      }
    } else if (it->first == kNetworkPropertyID) {
      parsed[flimflam::kNetworkIdProperty] = it->second;
    } else if (it->first == kNetworkPropertyLongName) {
      parsed[flimflam::kLongNameProperty] = it->second;
    } else if (it->first == kNetworkPropertyShortName) {
      parsed[flimflam::kShortNameProperty] = it->second;
    } else if (it->first == kNetworkPropertyAccessTechnology) {
      int tech = 0;
      if (base::StringToInt(it->second, &tech) &&
          tech >= 0 &&
          tech < static_cast<int>(arraysize(kTechnologyString))) {
        parsed[flimflam::kTechnologyProperty] = kTechnologyString[tech];
      } else {
        LOG(ERROR) << "Unexpected technology value: " << it->second;
      }
    } else {
      LOG(WARNING) << "Unknown network property ignored: " << it->first;
    }
  }
  // If the long name is not available but the network ID is, look up the long
  // name in the mobile provider database.
  if ((!ContainsKey(parsed, flimflam::kLongNameProperty) ||
       parsed[flimflam::kLongNameProperty].empty()) &&
      ContainsKey(parsed, flimflam::kNetworkIdProperty)) {
    mobile_provider *provider =
        mobile_provider_lookup_by_network(
            cellular()->provider_db(),
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

void CellularCapabilityGSM::SetAccessTechnology(uint32 access_technology) {
  access_technology_ = access_technology;
  if (cellular()->service().get()) {
    cellular()->service()->SetNetworkTechnology(GetNetworkTechnologyString());
  }
}

string CellularCapabilityGSM::GetNetworkTechnologyString() const {
  switch (access_technology_) {
    case MM_MODEM_GSM_ACCESS_TECH_GSM:
    case MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT:
      return flimflam::kNetworkTechnologyGsm;
    case MM_MODEM_GSM_ACCESS_TECH_GPRS:
      return flimflam::kNetworkTechnologyGprs;
    case MM_MODEM_GSM_ACCESS_TECH_EDGE:
      return flimflam::kNetworkTechnologyEdge;
    case MM_MODEM_GSM_ACCESS_TECH_UMTS:
      return flimflam::kNetworkTechnologyUmts;
    case MM_MODEM_GSM_ACCESS_TECH_HSDPA:
    case MM_MODEM_GSM_ACCESS_TECH_HSUPA:
    case MM_MODEM_GSM_ACCESS_TECH_HSPA:
      return flimflam::kNetworkTechnologyHspa;
    case MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS:
      return flimflam::kNetworkTechnologyHspaPlus;
    default:
      break;
  }
  return "";
}

string CellularCapabilityGSM::GetRoamingStateString() const {
  switch (registration_state_) {
    case MM_MODEM_GSM_NETWORK_REG_STATUS_HOME:
      return flimflam::kRoamingStateHome;
    case MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING:
      return flimflam::kRoamingStateRoaming;
    default:
      break;
  }
  return flimflam::kRoamingStateUnknown;
}

void CellularCapabilityGSM::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &properties,
    const vector<string> &invalidated_properties) {
  CellularCapabilityClassic::OnDBusPropertiesChanged(interface,
                                                     properties,
                                                     invalidated_properties);
  if (interface == MM_MODEM_GSM_NETWORK_INTERFACE) {
    uint32 access_technology = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
    if (DBusProperties::GetUint32(properties,
                                  kPropertyAccessTechnology,
                                  &access_technology)) {
      SetAccessTechnology(access_technology);
    }
  } else {
    bool emit = false;
    if (interface == MM_MODEM_GSM_CARD_INTERFACE) {
      uint32 locks = 0;
      if (DBusProperties::GetUint32(
              properties, kPropertyEnabledFacilityLocks, &locks)) {
        sim_lock_status_.enabled = locks & MM_MODEM_GSM_FACILITY_SIM;
        emit = true;
      }
    } else if (interface == MM_MODEM_INTERFACE) {
      if (DBusProperties::GetString(properties, kPropertyUnlockRequired,
                                    &sim_lock_status_.lock_type)) {
        emit = true;
      }
      if (DBusProperties::GetUint32(properties, kPropertyUnlockRetries,
                                    &sim_lock_status_.retries_left)) {
        emit = true;
      }
    }
    if (emit) {
      cellular()->adaptor()->EmitKeyValueStoreChanged(
          flimflam::kSIMLockStatusProperty, SimLockStatusToProperty(NULL));
    }
  }
}

void CellularCapabilityGSM::OnNetworkModeSignal(uint32 /*mode*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void CellularCapabilityGSM::OnRegistrationInfoSignal(
    uint32 status, const string &operator_code, const string &operator_name) {
  SLOG(Cellular, 2) << __func__ << ": regstate=" << status
                    << ", opercode=" << operator_code
                    << ", opername=" << operator_name;
  registration_state_ = status;
  serving_operator_.SetCode(operator_code);
  serving_operator_.SetName(operator_name);
  UpdateOperatorInfo();
  cellular()->HandleNewRegistrationState();
}

void CellularCapabilityGSM::OnSignalQualitySignal(uint32 quality) {
  cellular()->HandleNewSignalQuality(quality);
}

void CellularCapabilityGSM::OnGetRegistrationInfoReply(
    uint32 status, const string &operator_code, const string &operator_name,
    const Error &error) {
  if (error.IsSuccess())
    OnRegistrationInfoSignal(status, operator_code, operator_name);
}

void CellularCapabilityGSM::OnGetSignalQualityReply(uint32 quality,
                                                    const Error &error) {
  if (error.IsSuccess())
    OnSignalQualitySignal(quality);
}

void CellularCapabilityGSM::OnGetIMEIReply(const ResultCallback &callback,
                                           const string &imei,
                                           const Error &error) {
  if (error.IsSuccess()) {
    SLOG(Cellular, 2) << "IMEI: " << imei;
    imei_ = imei;
  } else {
    SLOG(Cellular, 2) << "GetIMEI failed - " << error;
  }
  callback.Run(error);
}

void CellularCapabilityGSM::OnGetIMSIReply(const ResultCallback &callback,
                                           const string &imsi,
                                           const Error &error) {
  if (error.IsSuccess()) {
    SLOG(Cellular, 2) << "IMSI: " << imsi;
    imsi_ = imsi;
    SetHomeProvider();
    callback.Run(error);
  } else {
    if (get_imsi_retries_++ < kGetIMSIRetryLimit) {
      SLOG(Cellular, 2) << "GetIMSI failed - " << error << ". Retrying";
      base::Callback<void(void)> retry_get_imsi_cb =
          Bind(&CellularCapabilityGSM::GetIMSI,
               weak_ptr_factory_.GetWeakPtr(), callback);
      cellular()->dispatcher()->PostDelayedTask(
          retry_get_imsi_cb,
          get_imsi_retry_delay_milliseconds_);
    } else {
      LOG(INFO) << "GetIMSI failed - " << error;
      callback.Run(error);
    }
  }
}

void CellularCapabilityGSM::OnGetSPNReply(const ResultCallback &callback,
                                          const string &spn,
                                          const Error &error) {
  if (error.IsSuccess()) {
    SLOG(Cellular, 2) << "SPN: " << spn;
    spn_ = spn;
    SetHomeProvider();
  } else {
    SLOG(Cellular, 2) << "GetSPN failed - " << error;
  }
  callback.Run(error);
}

void CellularCapabilityGSM::OnGetMSISDNReply(const ResultCallback &callback,
                                             const string &msisdn,
                                             const Error &error) {
  if (error.IsSuccess()) {
    SLOG(Cellular, 2) << "MSISDN: " << msisdn;
    mdn_ = msisdn;
  } else {
    SLOG(Cellular, 2) << "GetMSISDN failed - " << error;
  }
  callback.Run(error);
}

}  // namespace shill
