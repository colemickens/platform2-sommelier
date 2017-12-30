//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/cellular/cellular_capability_gsm.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/property_accessor.h"

using base::Bind;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kCellular;
static string ObjectID(CellularCapabilityGsm* c) {
  return c->cellular()->GetRpcIdentifier();
}
}

// static
const char CellularCapabilityGsm::kNetworkPropertyAccessTechnology[] =
    "access-tech";
const char CellularCapabilityGsm::kNetworkPropertyID[] = "operator-num";
const char CellularCapabilityGsm::kNetworkPropertyLongName[] = "operator-long";
const char CellularCapabilityGsm::kNetworkPropertyShortName[] =
    "operator-short";
const char CellularCapabilityGsm::kNetworkPropertyStatus[] = "status";
const char CellularCapabilityGsm::kPhoneNumber[] = "*99#";
const char CellularCapabilityGsm::kPropertyAccessTechnology[] =
    "AccessTechnology";
const char CellularCapabilityGsm::kPropertyEnabledFacilityLocks[] =
    "EnabledFacilityLocks";
const char CellularCapabilityGsm::kPropertyUnlockRequired[] = "UnlockRequired";
const char CellularCapabilityGsm::kPropertyUnlockRetries[] = "UnlockRetries";

const int CellularCapabilityGsm::kGetIMSIRetryLimit = 40;
const int64_t CellularCapabilityGsm::kGetIMSIRetryDelayMilliseconds = 500;

CellularCapabilityGsm::CellularCapabilityGsm(Cellular* cellular,
                                             ModemInfo* modem_info)
    : CellularCapabilityClassic(cellular, modem_info),
      mobile_operator_info_(
          new MobileOperatorInfo(cellular->dispatcher(), "ParseScanResult")),
      registration_state_(MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN),
      access_technology_(MM_MODEM_GSM_ACCESS_TECH_UNKNOWN),
      home_provider_info_(nullptr),
      get_imsi_retries_(0),
      get_imsi_retry_delay_milliseconds_(kGetIMSIRetryDelayMilliseconds),
      weak_ptr_factory_(this) {
  SLOG(this, 2) << "Cellular capability constructed: GSM";
  mobile_operator_info_->Init();
  HelpRegisterConstDerivedKeyValueStore(
      kSIMLockStatusProperty, &CellularCapabilityGsm::SimLockStatusToProperty);
  this->cellular()->set_scanning_supported(true);

  // TODO(benchan): This is a hack to initialize the GSM card proxy for GetIMSI
  // before InitProxies is called. There are side-effects of calling InitProxies
  // before the device is enabled. It's better to refactor InitProxies such that
  // proxies can be created when the cellular device/capability is constructed,
  // but callbacks for DBus signal updates are not set up until the device is
  // enabled.
  card_proxy_ = control_interface()->CreateModemGsmCardProxy(
      cellular->dbus_path(), cellular->dbus_service());
  // TODO(benchan): To allow unit testing using a mock proxy without further
  // complicating the code, the test proxy factory is set up to return a nullptr
  // pointer when CellularCapabilityGsm is constructed. Refactor the code to
  // avoid this hack.
  if (card_proxy_.get())
    InitProperties();
}

CellularCapabilityGsm::~CellularCapabilityGsm() {}

string CellularCapabilityGsm::GetTypeString() const {
  return kTechnologyFamilyGsm;
}

KeyValueStore CellularCapabilityGsm::SimLockStatusToProperty(Error* /*error*/) {
  KeyValueStore status;
  status.SetBool(kSIMLockEnabledProperty, sim_lock_status_.enabled);
  status.SetString(kSIMLockTypeProperty, sim_lock_status_.lock_type);
  status.SetInt(kSIMLockRetriesLeftProperty, sim_lock_status_.retries_left);
  return status;
}

void CellularCapabilityGsm::HelpRegisterConstDerivedKeyValueStore(
    const string& name,
    KeyValueStore(CellularCapabilityGsm::*get)(Error* error)) {
  cellular()->mutable_store()->RegisterDerivedKeyValueStore(
      name,
      KeyValueStoreAccessor(
          new CustomAccessor<CellularCapabilityGsm, KeyValueStore>(
              this, get, nullptr)));
}

void CellularCapabilityGsm::InitProxies() {
  CellularCapabilityClassic::InitProxies();
  // TODO(benchan): Remove this check after refactoring the proxy
  // initialization.
  if (!card_proxy_.get()) {
    card_proxy_ = control_interface()->CreateModemGsmCardProxy(
        cellular()->dbus_path(), cellular()->dbus_service());
  }
  network_proxy_ = control_interface()->CreateModemGsmNetworkProxy(
      cellular()->dbus_path(), cellular()->dbus_service());
  network_proxy_->set_signal_quality_callback(
      Bind(&CellularCapabilityGsm::OnSignalQualitySignal,
           weak_ptr_factory_.GetWeakPtr()));
  network_proxy_->set_network_mode_callback(
      Bind(&CellularCapabilityGsm::OnNetworkModeSignal,
           weak_ptr_factory_.GetWeakPtr()));
  network_proxy_->set_registration_info_callback(
      Bind(&CellularCapabilityGsm::OnRegistrationInfoSignal,
           weak_ptr_factory_.GetWeakPtr()));
}

void CellularCapabilityGsm::InitProperties() {
  CellularTaskList* tasks = new CellularTaskList();
  ResultCallback cb_ignore_error =
      Bind(&CellularCapabilityGsm::StepCompletedCallback,
           weak_ptr_factory_.GetWeakPtr(), ResultCallback(), true, tasks);
  // Chrome checks if a SIM is present before allowing the modem to be enabled,
  // so shill needs to obtain IMSI, as an indicator of SIM presence, even
  // before the device is enabled.
  tasks->push_back(Bind(&CellularCapabilityGsm::GetIMSI,
                        weak_ptr_factory_.GetWeakPtr(), cb_ignore_error));
  RunNextStep(tasks);
}

void CellularCapabilityGsm::StartModem(Error* error,
                                       const ResultCallback& callback) {
  InitProxies();

  CellularTaskList* tasks = new CellularTaskList();
  ResultCallback cb =
      Bind(&CellularCapabilityGsm::StepCompletedCallback,
           weak_ptr_factory_.GetWeakPtr(), callback, false, tasks);
  ResultCallback cb_ignore_error =
        Bind(&CellularCapabilityGsm::StepCompletedCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback, true, tasks);
  if (!cellular()->IsUnderlyingDeviceEnabled())
    tasks->push_back(Bind(&CellularCapabilityGsm::EnableModem,
                          weak_ptr_factory_.GetWeakPtr(), cb));
  // If we're within range of the home network, the modem will try to
  // register once it's enabled, or may be already registered if we
  // started out enabled.
  if (!IsUnderlyingDeviceRegistered() &&
      !cellular()->selected_network().empty())
    tasks->push_back(Bind(&CellularCapabilityGsm::Register,
                          weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityGsm::GetIMEI,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  get_imsi_retries_ = 0;
  tasks->push_back(Bind(&CellularCapabilityGsm::GetIMSI,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityGsm::GetSPN,
                        weak_ptr_factory_.GetWeakPtr(), cb_ignore_error));
  tasks->push_back(Bind(&CellularCapabilityGsm::GetMSISDN,
                        weak_ptr_factory_.GetWeakPtr(), cb_ignore_error));
  tasks->push_back(Bind(&CellularCapabilityGsm::GetProperties,
                        weak_ptr_factory_.GetWeakPtr(), cb));
  tasks->push_back(Bind(&CellularCapabilityGsm::GetModemInfo,
                        weak_ptr_factory_.GetWeakPtr(), cb_ignore_error));
  tasks->push_back(Bind(&CellularCapabilityGsm::FinishEnable,
                        weak_ptr_factory_.GetWeakPtr(), cb));

  RunNextStep(tasks);
}

bool CellularCapabilityGsm::IsUnderlyingDeviceRegistered() const {
  switch (cellular()->modem_state()) {
    case Cellular::kModemStateFailed:
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

void CellularCapabilityGsm::ReleaseProxies() {
  SLOG(this, 2) << __func__;
  CellularCapabilityClassic::ReleaseProxies();
  card_proxy_.reset();
  network_proxy_.reset();
}

bool CellularCapabilityGsm::AreProxiesInitialized() const {
  return (CellularCapabilityClassic::AreProxiesInitialized() &&
          card_proxy_.get() && network_proxy_.get());
}

void CellularCapabilityGsm::OnServiceCreated() {
  cellular()->service()->SetActivationState(kActivationStateActivated);
}

// Create the list of APNs to try, in the following order:
// - the APN, if any, that was set by the user
// - last APN that resulted in a successful connection attempt on the
//   current network (if any)
// - the list of APNs found in the mobile broadband provider DB for the
//   home provider associated with the current SIM
// - as a last resort, attempt to connect with no APN
void CellularCapabilityGsm::SetupApnTryList() {
  apn_try_list_.clear();

  DCHECK(cellular()->service().get());
  const Stringmap* apn_info = cellular()->service()->GetUserSpecifiedApn();
  if (apn_info)
    apn_try_list_.push_back(*apn_info);

  apn_info = cellular()->service()->GetLastGoodApn();
  if (apn_info)
    apn_try_list_.push_back(*apn_info);

  apn_try_list_.insert(apn_try_list_.end(),
                       cellular()->apn_list().begin(),
                       cellular()->apn_list().end());
}

void CellularCapabilityGsm::SetupConnectProperties(
    KeyValueStore* properties) {
  SetupApnTryList();
  FillConnectPropertyMap(properties);
}

void CellularCapabilityGsm::FillConnectPropertyMap(
    KeyValueStore* properties) {
  properties->SetString(kConnectPropertyPhoneNumber, kPhoneNumber);

  if (!cellular()->IsRoamingAllowedOrRequired())
    properties->SetBool(kConnectPropertyHomeOnly, true);

  if (!apn_try_list_.empty()) {
    // Leave the APN at the front of the list, so that it can be recorded
    // if the connect attempt succeeds.
    Stringmap apn_info = apn_try_list_.front();
    SLOG(this, 2) << __func__ << ": Using APN " << apn_info[kApnProperty];
    properties->SetString(kConnectPropertyApn, apn_info[kApnProperty]);
    if (ContainsKey(apn_info, kApnUsernameProperty))
      properties->SetString(kConnectPropertyApnUsername,
                            apn_info[kApnUsernameProperty]);
    if (ContainsKey(apn_info, kApnPasswordProperty))
      properties->SetString(kConnectPropertyApnPassword,
                            apn_info[kApnPasswordProperty]);
  }
}

void CellularCapabilityGsm::Connect(const KeyValueStore& properties,
                                    Error* error,
                                    const ResultCallback& callback) {
  SLOG(this, 2) << __func__;
  ResultCallback cb = Bind(&CellularCapabilityGsm::OnConnectReply,
                           weak_ptr_factory_.GetWeakPtr(),
                           callback);
  simple_proxy_->Connect(properties, error, cb, kTimeoutConnect);
}

void CellularCapabilityGsm::OnConnectReply(const ResultCallback& callback,
                                           const Error& error) {
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
      SLOG(this, 2) << "Connect failed with invalid APN, "
                    << apn_try_list_.size() << " remaining APNs to try";
      KeyValueStore props;
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

// always called from an async context
void CellularCapabilityGsm::GetIMEI(const ResultCallback& callback) {
  SLOG(this, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  if (cellular()->imei().empty()) {
    GsmIdentifierCallback cb = Bind(&CellularCapabilityGsm::OnGetIMEIReply,
                                    weak_ptr_factory_.GetWeakPtr(), callback);
    card_proxy_->GetIMEI(&error, cb, kTimeoutDefault);
    if (error.IsFailure())
      callback.Run(error);
  } else {
    SLOG(this, 2) << "Already have IMEI " << cellular()->imei();
    callback.Run(error);
  }
}

// always called from an async context
void CellularCapabilityGsm::GetIMSI(const ResultCallback& callback) {
  SLOG(this, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  if (cellular()->imsi().empty()) {
    GsmIdentifierCallback cb = Bind(&CellularCapabilityGsm::OnGetIMSIReply,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback);
    card_proxy_->GetIMSI(&error, cb, kTimeoutDefault);
    if (error.IsFailure()) {
      cellular()->home_provider_info()->Reset();
      callback.Run(error);
    }
  } else {
    SLOG(this, 2) << "Already have IMSI " << cellular()->imsi();
    callback.Run(error);
  }
}

// always called from an async context
void CellularCapabilityGsm::GetSPN(const ResultCallback& callback) {
  SLOG(this, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  if (spn_.empty()) {
    GsmIdentifierCallback cb = Bind(&CellularCapabilityGsm::OnGetSPNReply,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback);
    card_proxy_->GetSPN(&error, cb, kTimeoutDefault);
    if (error.IsFailure())
      callback.Run(error);
  } else {
    SLOG(this, 2) << "Already have SPN " << spn_;
    callback.Run(error);
  }
}

// always called from an async context
void CellularCapabilityGsm::GetMSISDN(const ResultCallback& callback) {
  SLOG(this, 2) << __func__;
  CHECK(!callback.is_null());
  Error error;
  string mdn = cellular()->mdn();
  if (mdn.empty()) {
    GsmIdentifierCallback cb = Bind(&CellularCapabilityGsm::OnGetMSISDNReply,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback);
    card_proxy_->GetMSISDN(&error, cb, kTimeoutDefault);
    if (error.IsFailure())
      callback.Run(error);
  } else {
    SLOG(this, 2) << "Already have MSISDN " << mdn;
    callback.Run(error);
  }
}

void CellularCapabilityGsm::GetSignalQuality() {
  SLOG(this, 2) << __func__;
  SignalQualityCallback callback =
      Bind(&CellularCapabilityGsm::OnGetSignalQualityReply,
           weak_ptr_factory_.GetWeakPtr());
  network_proxy_->GetSignalQuality(nullptr, callback, kTimeoutDefault);
}

void CellularCapabilityGsm::GetRegistrationState() {
  SLOG(this, 2) << __func__;
  RegistrationInfoCallback callback =
      Bind(&CellularCapabilityGsm::OnGetRegistrationInfoReply,
           weak_ptr_factory_.GetWeakPtr());
  network_proxy_->GetRegistrationInfo(nullptr, callback, kTimeoutDefault);
}

void CellularCapabilityGsm::GetProperties(const ResultCallback& callback) {
  SLOG(this, 2) << __func__;

  // TODO(petkov): Switch to asynchronous calls (crbug.com/200687).
  uint32_t tech = network_proxy_->AccessTechnology();
  SetAccessTechnology(tech);
  SLOG(this, 2) << "GSM AccessTechnology: " << tech;

  // TODO(petkov): Switch to asynchronous calls (crbug.com/200687).
  uint32_t locks = card_proxy_->EnabledFacilityLocks();
  sim_lock_status_.enabled = locks & MM_MODEM_GSM_FACILITY_SIM;
  SLOG(this, 2) << "GSM EnabledFacilityLocks: " << locks;

  callback.Run(Error());
}

// always called from an async context
void CellularCapabilityGsm::Register(const ResultCallback& callback) {
  SLOG(this, 2) << __func__ << " \"" << cellular()->selected_network()
                << "\"";
  CHECK(!callback.is_null());
  Error error;
  ResultCallback cb = Bind(&CellularCapabilityGsm::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  network_proxy_->Register(cellular()->selected_network(), &error, cb,
                           kTimeoutRegister);
  if (error.IsFailure())
    callback.Run(error);
}

void CellularCapabilityGsm::RegisterOnNetwork(
    const string& network_id,
    Error* error,
    const ResultCallback& callback) {
  SLOG(this, 2) << __func__ << "(" << network_id << ")";
  CHECK(error);
  desired_network_ = network_id;
  ResultCallback cb = Bind(&CellularCapabilityGsm::OnRegisterReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  network_proxy_->Register(network_id, error, cb, kTimeoutRegister);
}

void CellularCapabilityGsm::OnRegisterReply(const ResultCallback& callback,
                                            const Error& error) {
  SLOG(this, 2) << __func__ << "(" << error << ")";

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

bool CellularCapabilityGsm::IsRegistered() const {
  return (registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME ||
          registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
}

void CellularCapabilityGsm::SetUnregistered(bool searching) {
  // If we're already in some non-registered state, don't override that
  if (registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME ||
      registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING) {
    registration_state_ =
        (searching ? MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING :
                     MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE);
  }
}

void CellularCapabilityGsm::RequirePIN(
    const std::string& pin, bool require,
    Error* error, const ResultCallback& callback) {
  CHECK(error);
  card_proxy_->EnablePIN(pin, require, error, callback, kTimeoutDefault);
}

void CellularCapabilityGsm::EnterPIN(const string& pin,
                                     Error* error,
                                     const ResultCallback& callback) {
  CHECK(error);
  card_proxy_->SendPIN(pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityGsm::UnblockPIN(const string& unblock_code,
                                       const string& pin,
                                       Error* error,
                                       const ResultCallback& callback) {
  CHECK(error);
  card_proxy_->SendPUK(unblock_code, pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityGsm::ChangePIN(
    const string& old_pin, const string& new_pin,
    Error* error, const ResultCallback& callback) {
  CHECK(error);
  card_proxy_->ChangePIN(old_pin, new_pin, error, callback, kTimeoutDefault);
}

void CellularCapabilityGsm::Scan(Error* error,
                                 const ResultStringmapsCallback& callback) {
  ScanResultsCallback cb = Bind(&CellularCapabilityGsm::OnScanReply,
                                weak_ptr_factory_.GetWeakPtr(), callback);
  network_proxy_->Scan(error, cb, kTimeoutScan);
}

void CellularCapabilityGsm::OnScanReply(
    const ResultStringmapsCallback& callback,
    const GsmScanResults& results,
    const Error& error) {
  Stringmaps found_networks;
  for (const auto& result : results)
    found_networks.push_back(ParseScanResult(result));
  callback.Run(found_networks, error);
}

Stringmap CellularCapabilityGsm::ParseScanResult(const GsmScanResult& result) {
  Stringmap parsed;
  for (const auto& property_key_value_pair : result) {
    const string& property_key = property_key_value_pair.first;
    const string& property_value = property_key_value_pair.second;

    // TODO(petkov): Define these in system_api/service_constants.h. The
    // numerical values are taken from 3GPP TS 27.007 Section 7.3.
    static const char* const kStatusString[] = {
      "unknown",
      "available",
      "current",
      "forbidden",
    };
    static const char* const kTechnologyString[] = {
      kNetworkTechnologyGsm,
      "GSM Compact",
      kNetworkTechnologyUmts,
      kNetworkTechnologyEdge,
      "HSDPA",
      "HSUPA",
      kNetworkTechnologyHspa,
    };
    SLOG(this, 2) << "Network property: " << property_key << " = "
                  << property_value;
    if (property_key == kNetworkPropertyStatus) {
      int status = 0;
      if (base::StringToInt(property_value, &status) &&
          status >= 0 &&
          status < static_cast<int>(arraysize(kStatusString))) {
        parsed[kStatusProperty] = kStatusString[status];
      } else {
        LOG(ERROR) << "Unexpected status value: " << property_value;
      }
    } else if (property_key == kNetworkPropertyID) {
      parsed[kNetworkIdProperty] = property_value;
    } else if (property_key == kNetworkPropertyLongName) {
      parsed[kLongNameProperty] = property_value;
    } else if (property_key == kNetworkPropertyShortName) {
      parsed[kShortNameProperty] = property_value;
    } else if (property_key == kNetworkPropertyAccessTechnology) {
      int tech = 0;
      if (base::StringToInt(property_value, &tech) &&
          tech >= 0 &&
          tech < static_cast<int>(arraysize(kTechnologyString))) {
        parsed[kTechnologyProperty] = kTechnologyString[tech];
      } else {
        LOG(ERROR) << "Unexpected technology value: " << property_value;
      }
    } else {
      LOG(WARNING) << "Unknown network property ignored: " << property_key;
    }
  }
  // If the long name is not available but the network ID is, look up the long
  // name in the mobile provider database.
  if ((!ContainsKey(parsed, kLongNameProperty) ||
       parsed[kLongNameProperty].empty()) &&
      ContainsKey(parsed, kNetworkIdProperty)) {
    mobile_operator_info_->Reset();
    mobile_operator_info_->UpdateMCCMNC(parsed[kNetworkIdProperty]);
    if (mobile_operator_info_->IsMobileNetworkOperatorKnown() &&
        !mobile_operator_info_->operator_name().empty()) {
      parsed[kLongNameProperty] = mobile_operator_info_->operator_name();
    }
  }
  return parsed;
}

void CellularCapabilityGsm::SetAccessTechnology(uint32_t access_technology) {
  access_technology_ = access_technology;
  if (cellular()->service().get()) {
    cellular()->service()->SetNetworkTechnology(GetNetworkTechnologyString());
  }
}

string CellularCapabilityGsm::GetNetworkTechnologyString() const {
  switch (access_technology_) {
    case MM_MODEM_GSM_ACCESS_TECH_GSM:
    case MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT:
      return kNetworkTechnologyGsm;
    case MM_MODEM_GSM_ACCESS_TECH_GPRS:
      return kNetworkTechnologyGprs;
    case MM_MODEM_GSM_ACCESS_TECH_EDGE:
      return kNetworkTechnologyEdge;
    case MM_MODEM_GSM_ACCESS_TECH_UMTS:
      return kNetworkTechnologyUmts;
    case MM_MODEM_GSM_ACCESS_TECH_HSDPA:
    case MM_MODEM_GSM_ACCESS_TECH_HSUPA:
    case MM_MODEM_GSM_ACCESS_TECH_HSPA:
      return kNetworkTechnologyHspa;
    case MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS:
      return kNetworkTechnologyHspaPlus;
    default:
      break;
  }
  return "";
}

string CellularCapabilityGsm::GetRoamingStateString() const {
  switch (registration_state_) {
    case MM_MODEM_GSM_NETWORK_REG_STATUS_HOME:
      return kRoamingStateHome;
    case MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING:
      return kRoamingStateRoaming;
    default:
      break;
  }
  return kRoamingStateUnknown;
}

void CellularCapabilityGsm::OnPropertiesChanged(
    const string& interface,
    const KeyValueStore& properties,
    const vector<string>& invalidated_properties) {
  CellularCapabilityClassic::OnPropertiesChanged(interface,
                                                 properties,
                                                 invalidated_properties);
  if (interface == MM_MODEM_GSM_NETWORK_INTERFACE) {
    if (properties.ContainsUint(kPropertyAccessTechnology)) {
      SetAccessTechnology(properties.GetUint(kPropertyAccessTechnology));
    }
  } else {
    bool emit = false;
    if (interface == MM_MODEM_GSM_CARD_INTERFACE) {
      if (properties.ContainsUint(kPropertyEnabledFacilityLocks)) {
        uint32_t locks = properties.GetUint(kPropertyEnabledFacilityLocks);
        sim_lock_status_.enabled = locks & MM_MODEM_GSM_FACILITY_SIM;
        emit = true;
      }
    } else if (interface == MM_MODEM_INTERFACE) {
      if (properties.ContainsString(kPropertyUnlockRequired)) {
        sim_lock_status_.lock_type =
            properties.GetString(kPropertyUnlockRequired);
        emit = true;
      }
      if (properties.ContainsUint(kPropertyUnlockRetries)) {
        sim_lock_status_.retries_left =
            properties.GetUint(kPropertyUnlockRetries);
        emit = true;
      }
    }
    // TODO(pprabhu) Rename |emit| to |sim_present| after |sim_lock_status|
    // moves to cellular.
    if (emit) {
      cellular()->set_sim_present(true);
      cellular()->adaptor()->EmitKeyValueStoreChanged(
          kSIMLockStatusProperty, SimLockStatusToProperty(nullptr));
    }
  }
}

void CellularCapabilityGsm::OnNetworkModeSignal(uint32_t /*mode*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void CellularCapabilityGsm::OnRegistrationInfoSignal(
    uint32_t status, const string& operator_code, const string& operator_name) {
  SLOG(this, 2) << __func__ << ": regstate=" << status
                << ", opercode=" << operator_code
                << ", opername=" << operator_name;
  registration_state_ = status;
  cellular()->serving_operator_info()->UpdateMCCMNC(operator_code);
  cellular()->serving_operator_info()->UpdateOperatorName(operator_name);
  cellular()->HandleNewRegistrationState();
}

void CellularCapabilityGsm::OnSignalQualitySignal(uint32_t quality) {
  cellular()->HandleNewSignalQuality(quality);
}

void CellularCapabilityGsm::OnGetRegistrationInfoReply(
    uint32_t status, const string& operator_code, const string& operator_name,
    const Error& error) {
  if (error.IsSuccess())
    OnRegistrationInfoSignal(status, operator_code, operator_name);
}

void CellularCapabilityGsm::OnGetSignalQualityReply(uint32_t quality,
                                                    const Error& error) {
  if (error.IsSuccess())
    OnSignalQualitySignal(quality);
}

void CellularCapabilityGsm::OnGetIMEIReply(const ResultCallback& callback,
                                           const string& imei,
                                           const Error& error) {
  if (error.IsSuccess()) {
    SLOG(this, 2) << "IMEI: " << imei;
    cellular()->set_imei(imei);
  } else {
    SLOG(this, 2) << "GetIMEI failed - " << error;
  }
  callback.Run(error);
}

void CellularCapabilityGsm::OnGetIMSIReply(const ResultCallback& callback,
                                           const string& imsi,
                                           const Error& error) {
  if (error.IsSuccess()) {
    SLOG(this, 2) << "IMSI: " << imsi;
    cellular()->set_imsi(imsi);
    cellular()->set_sim_present(true);
    cellular()->home_provider_info()->UpdateIMSI(imsi);
    // We do not currently obtain the IMSI OTA at all. Provide the IMSI from the
    // SIM to the serving operator as well to aid in MVNO identification.
    cellular()->serving_operator_info()->UpdateIMSI(imsi);
    callback.Run(error);
  } else if (!sim_lock_status_.lock_type.empty()) {
    SLOG(this, 2) << "GetIMSI failed - SIM lock in place.";
    cellular()->set_sim_present(true);
    callback.Run(error);
  } else {
    cellular()->set_sim_present(false);
    if (get_imsi_retries_++ < kGetIMSIRetryLimit) {
      SLOG(this, 2) << "GetIMSI failed - " << error << ". Retrying";
      base::Closure retry_get_imsi_cb = Bind(&CellularCapabilityGsm::GetIMSI,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             callback);
      cellular()->dispatcher()->PostDelayedTask(
          FROM_HERE,
          retry_get_imsi_cb,
          get_imsi_retry_delay_milliseconds_);
    } else {
      LOG(INFO) << "GetIMSI failed - " << error;
      cellular()->home_provider_info()->Reset();
      callback.Run(error);
    }
  }
}

void CellularCapabilityGsm::OnGetSPNReply(const ResultCallback& callback,
                                          const string& spn,
                                          const Error& error) {
  if (error.IsSuccess()) {
    SLOG(this, 2) << "SPN: " << spn;
    spn_ = spn;
    cellular()->home_provider_info()->UpdateOperatorName(spn);
  } else {
    SLOG(this, 2) << "GetSPN failed - " << error;
  }
  callback.Run(error);
}

void CellularCapabilityGsm::OnGetMSISDNReply(const ResultCallback& callback,
                                             const string& msisdn,
                                             const Error& error) {
  if (error.IsSuccess()) {
    SLOG(this, 2) << "MSISDN: " << msisdn;
    cellular()->set_mdn(msisdn);
  } else {
    SLOG(this, 2) << "GetMSISDN failed - " << error;
  }
  callback.Run(error);
}

}  // namespace shill
