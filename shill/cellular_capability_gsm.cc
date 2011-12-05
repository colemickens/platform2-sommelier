// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <base/logging.h>
#include <base/stl_util-inl.h>
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

using std::make_pair;
using std::string;

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
const char CellularCapabilityGSM::kPropertyUnlockRequired[] = "UnlockRequired";
const char CellularCapabilityGSM::kPropertyUnlockRetries[] = "UnlockRetries";

CellularCapabilityGSM::CellularCapabilityGSM(Cellular *cellular,
                                             ProxyFactory *proxy_factory)
    : CellularCapability(cellular, proxy_factory),
      task_factory_(this),
      registration_state_(MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN),
      access_technology_(MM_MODEM_GSM_ACCESS_TECH_UNKNOWN),
      home_provider_(NULL),
      scanning_(false),
      scan_interval_(0) {
  VLOG(2) << "Cellular capability constructed: GSM";
  PropertyStore *store = cellular->mutable_store();
  store->RegisterConstString(flimflam::kSelectedNetworkProperty,
                             &selected_network_);
  store->RegisterConstStringmaps(flimflam::kFoundNetworksProperty,
                                 &found_networks_);
  store->RegisterConstBool(flimflam::kScanningProperty, &scanning_);
  store->RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);
  HelpRegisterDerivedStrIntPair(flimflam::kSIMLockStatusProperty,
                                &CellularCapabilityGSM::SimLockStatusToProperty,
                                NULL);
  store->RegisterConstStringmaps(flimflam::kCellularApnListProperty,
                                 &apn_list_);
}

StrIntPair CellularCapabilityGSM::SimLockStatusToProperty(Error */*error*/) {
  return StrIntPair(make_pair(flimflam::kSIMLockTypeProperty,
                              sim_lock_status_.lock_type),
                    make_pair(flimflam::kSIMLockRetriesLeftProperty,
                              sim_lock_status_.retries_left));
}

void CellularCapabilityGSM::HelpRegisterDerivedStrIntPair(
    const string &name,
    StrIntPair(CellularCapabilityGSM::*get)(Error *),
    void(CellularCapabilityGSM::*set)(const StrIntPair &, Error *)) {
  cellular()->mutable_store()->RegisterDerivedStrIntPair(
      name,
      StrIntPairAccessor(
          new CustomAccessor<CellularCapabilityGSM, StrIntPair>(
              this, get, set)));
}

void CellularCapabilityGSM::StartModem()
{
  CellularCapability::StartModem();
  card_proxy_.reset(
      proxy_factory()->CreateModemGSMCardProxy(this,
                                               cellular()->dbus_path(),
                                               cellular()->dbus_owner()));
  network_proxy_.reset(
      proxy_factory()->CreateModemGSMNetworkProxy(this,
                                                  cellular()->dbus_path(),
                                                  cellular()->dbus_owner()));
  MultiStepAsyncCallHandler *call_handler =
      new MultiStepAsyncCallHandler(cellular()->dispatcher());
  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::EnableModem, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::Register, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetModemStatus, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetIMEI, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetIMSI, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetSPN, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetMSISDN, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetProperties, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetModemInfo, call_handler));

  call_handler->AddTask(task_factory_.NewRunnableMethod(
                      &CellularCapabilityGSM::GetRegistrationState,
                      call_handler));
  call_handler->PostNextTask();
}

void CellularCapabilityGSM::StopModem() {
  VLOG(2) << __func__;
  CellularCapability::StopModem();
  card_proxy_.reset();
  network_proxy_.reset();
}

void CellularCapabilityGSM::OnServiceCreated() {
  cellular()->service()->SetActivationState(
      flimflam::kActivationStateActivated);
  UpdateServingOperator();
}

void CellularCapabilityGSM::UpdateStatus(const DBusPropertiesMap &properties) {
  if (ContainsKey(properties, kPropertyIMSI)) {
    SetHomeProvider();
  }
}

void CellularCapabilityGSM::SetupConnectProperties(
    DBusPropertiesMap *properties) {
  (*properties)[kConnectPropertyPhoneNumber].writer().append_string(
      kPhoneNumber);
  // TODO(petkov): Setup apn and "home_only".
}

void CellularCapabilityGSM::GetIMEI(AsyncCallHandler *call_handler){
  VLOG(2) << __func__;
  if (imei_.empty()) {
    card_proxy_->GetIMEI(call_handler, kTimeoutDefault);
  } else {
    VLOG(2) << "Already have IMEI " << imei_;
    CompleteOperation(call_handler);
  }
}

void CellularCapabilityGSM::GetIMSI(AsyncCallHandler *call_handler){
  VLOG(2) << __func__;
  if (imsi_.empty()) {
    card_proxy_->GetIMSI(call_handler, kTimeoutDefault);
  } else {
    VLOG(2) << "Already have IMSI " << imsi_;
    CompleteOperation(call_handler);
  }
}

void CellularCapabilityGSM::GetSPN(AsyncCallHandler *call_handler){
  VLOG(2) << __func__;
  if (spn_.empty()) {
    card_proxy_->GetSPN(call_handler, kTimeoutDefault);
  } else {
    VLOG(2) << "Already have SPN " << spn_;
    CompleteOperation(call_handler);
  }
}

void CellularCapabilityGSM::GetMSISDN(AsyncCallHandler *call_handler){
  VLOG(2) << __func__;
  if (mdn_.empty()) {
    card_proxy_->GetMSISDN(call_handler, kTimeoutDefault);
  } else {
    VLOG(2) << "Already have MSISDN " << mdn_;
    CompleteOperation(call_handler);
  }
}

void CellularCapabilityGSM::GetSignalQuality() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 strength = network_proxy_->GetSignalQuality();
  cellular()->HandleNewSignalQuality(strength);
}

void CellularCapabilityGSM::GetRegistrationState(
    AsyncCallHandler *call_handler) {
  VLOG(2) << __func__;
  network_proxy_->GetRegistrationInfo(call_handler, kTimeoutDefault);
}

void CellularCapabilityGSM::GetProperties(AsyncCallHandler *call_handler) {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 tech = network_proxy_->AccessTechnology();
  SetAccessTechnology(tech);
  VLOG(2) << "GSM AccessTechnology: " << tech;
  CompleteOperation(call_handler);
}

string CellularCapabilityGSM::CreateFriendlyServiceName() {
  VLOG(2) << __func__;
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
  VLOG(2) << __func__ << "(IMSI: " << imsi_
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
    VLOG(2) << "GSM provider not found.";
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
  VLOG(2) << __func__;
  const string &network_id = serving_operator_.GetCode();
  if (!network_id.empty()) {
    VLOG(2) << "Looking up network id: " << network_id;
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
        VLOG(2) << "Operator name: " << serving_operator_.GetName()
                << ", country: " << serving_operator_.GetCountry();
      }
    } else {
      VLOG(2) << "GSM provider not found.";
    }
  }
  UpdateServingOperator();
}

void CellularCapabilityGSM::UpdateServingOperator() {
  VLOG(2) << __func__;
  if (cellular()->service().get()) {
    cellular()->service()->set_serving_operator(serving_operator_);
  }
}

void CellularCapabilityGSM::InitAPNList() {
  VLOG(2) << __func__;
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

void CellularCapabilityGSM::Register(AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << " \"" << selected_network_ << "\"";
  network_proxy_->Register(selected_network_, call_handler,
                           kTimeoutRegister);
}

void CellularCapabilityGSM::RegisterOnNetwork(
    const string &network_id, AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << network_id << ")";
  desired_network_ = network_id;
  network_proxy_->Register(network_id, call_handler,
                           kTimeoutRegister);
}

void CellularCapabilityGSM::OnRegisterCallback(const Error &error,
                                               AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << error << ")";

  if (error.IsSuccess()) {
    selected_network_ = desired_network_;
    desired_network_.clear();
    CompleteOperation(call_handler);
    return;
  }
  // If registration on the desired network failed,
  // try to register on the home network.
  if (!desired_network_.empty()) {
    desired_network_.clear();
    selected_network_.clear();
    Register(call_handler);
    return;
  }
  CompleteOperation(call_handler, error);
}

bool CellularCapabilityGSM::IsRegistered() {
  return (registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME ||
          registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
}

void CellularCapabilityGSM::RequirePIN(
    const string &pin, bool require, AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << call_handler << ")";
  card_proxy_->EnablePIN(pin, require, call_handler, kTimeoutDefault);
}

void CellularCapabilityGSM::EnterPIN(const string &pin,
                                     AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << call_handler << ")";
  card_proxy_->SendPIN(pin, call_handler, kTimeoutDefault);
}

void CellularCapabilityGSM::UnblockPIN(const string &unblock_code,
                                       const string &pin,
                                       AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << call_handler << ")";
  card_proxy_->SendPUK(unblock_code, pin, call_handler,
                       kTimeoutDefault);
}

void CellularCapabilityGSM::ChangePIN(
    const string &old_pin, const string &new_pin,
    AsyncCallHandler *call_handler) {
  VLOG(2) << __func__ << "(" << call_handler << ")";
  card_proxy_->ChangePIN(old_pin, new_pin, call_handler,
                         kTimeoutDefault);
}

void CellularCapabilityGSM::Scan(AsyncCallHandler *call_handler) {
  VLOG(2) << __func__;
  // TODO(petkov): Defer scan requests if a scan is in progress already.
  network_proxy_->Scan(call_handler, kTimeoutScan);
}

void CellularCapabilityGSM::OnScanCallback(const GSMScanResults &results,
                                           const Error &error,
                                           AsyncCallHandler *call_handler) {
  VLOG(2) << __func__;
  if (error.IsFailure()) {
    CompleteOperation(call_handler, error);
    return;
  }
  found_networks_.clear();
  for (GSMScanResults::const_iterator it = results.begin();
       it != results.end(); ++it) {
    found_networks_.push_back(ParseScanResult(*it));
  }
  CompleteOperation(call_handler);
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
    VLOG(2) << "Network property: " << it->first << " = " << it->second;
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

void CellularCapabilityGSM::OnModemManagerPropertiesChanged(
    const DBusPropertiesMap &properties) {
  uint32 access_technology = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
  if (DBusProperties::GetUint32(properties,
                                kPropertyAccessTechnology,
                                &access_technology)) {
    SetAccessTechnology(access_technology);
  }
  DBusProperties::GetString(
      properties, kPropertyUnlockRequired, &sim_lock_status_.lock_type);
  DBusProperties::GetUint32(
      properties, kPropertyUnlockRetries, &sim_lock_status_.retries_left);
}

void CellularCapabilityGSM::OnGSMNetworkModeChanged(uint32 /*mode*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void CellularCapabilityGSM::OnGSMRegistrationInfoChanged(
    uint32 status, const string &operator_code, const string &operator_name,
    const Error &error, AsyncCallHandler *call_handler) {
  if (error.IsSuccess()) {
    VLOG(2) << __func__ << ": regstate=" << status
            << ", opercode=" << operator_code
            << ", opername=" << operator_name;
    registration_state_ = status;
    serving_operator_.SetCode(operator_code);
    serving_operator_.SetName(operator_name);
    UpdateOperatorInfo();
    cellular()->HandleNewRegistrationState();
  }
  CompleteOperation(call_handler, error);
}

void CellularCapabilityGSM::OnGSMSignalQualityChanged(uint32 quality) {
  cellular()->HandleNewSignalQuality(quality);
}

void CellularCapabilityGSM::OnGetIMEICallback(const string &imei,
                                              const Error &error,
                                              AsyncCallHandler *call_handler) {
  if (error.IsSuccess()) {
    VLOG(2) << "IMEI: " << imei;
    imei_ = imei;
  } else {
    VLOG(2) << "GetIMEI failed - " << error;
  }
  CompleteOperation(call_handler, error);
}

void CellularCapabilityGSM::OnGetIMSICallback(const string &imsi,
                                              const Error &error,
                                              AsyncCallHandler *call_handler) {
  if (error.IsSuccess()) {
    VLOG(2) << "IMSI: " << imsi;
    imsi_ = imsi;
    SetHomeProvider();
  } else {
    VLOG(2) << "GetIMSI failed - " << error;
  }
  CompleteOperation(call_handler, error);
}

void CellularCapabilityGSM::OnGetSPNCallback(const string &spn,
                                             const Error &error,
                                             AsyncCallHandler *call_handler) {
  if (error.IsSuccess()) {
    VLOG(2) << "SPN: " << spn;
    spn_ = spn;
    SetHomeProvider();
  } else {
    VLOG(2) << "GetSPN failed - " << error;
  }
  // Ignore the error - it's not fatal.
  CompleteOperation(call_handler);
}

void CellularCapabilityGSM::OnGetMSISDNCallback(
    const string &msisdn, const Error &error, AsyncCallHandler *call_handler) {
  if (error.IsSuccess()) {
    VLOG(2) << "MSISDN: " << msisdn;
    mdn_ = msisdn;
  } else {
    VLOG(2) << "GetMSISDN failed - " << error;
  }
  // Ignore the error - it's not fatal.
  CompleteOperation(call_handler);
}

void CellularCapabilityGSM::OnPINOperationCallback(
    const Error &error, AsyncCallHandler *call_handler) {
  if (error.IsFailure())
    VLOG(2) << "PIN operation failed - " << error;
  else
    VLOG(2) << "PIN operation complete";
  CompleteOperation(call_handler, error);
}

}  // namespace shill
