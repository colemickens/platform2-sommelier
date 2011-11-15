// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <base/logging.h>
#include <base/stl_util-inl.h>
#include <base/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/proxy_factory.h"

using std::string;

namespace shill {

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

CellularCapabilityGSM::CellularCapabilityGSM(Cellular *cellular)
    : CellularCapability(cellular),
      task_factory_(this),
      registration_state_(MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN),
      access_technology_(MM_MODEM_GSM_ACCESS_TECH_UNKNOWN),
      scanning_(false),
      scan_interval_(0) {
  PropertyStore *store = cellular->mutable_store();
  store->RegisterConstString(flimflam::kSelectedNetworkProperty,
                             &selected_network_);
  store->RegisterConstStringmaps(flimflam::kFoundNetworksProperty,
                                 &found_networks_);
  store->RegisterConstBool(flimflam::kScanningProperty, &scanning_);
  store->RegisterUint16(flimflam::kScanIntervalProperty, &scan_interval_);
}

void CellularCapabilityGSM::InitProxies() {
  VLOG(2) << __func__;
  card_proxy_.reset(
      proxy_factory()->CreateModemGSMCardProxy(this,
                                               cellular()->dbus_path(),
                                               cellular()->dbus_owner()));
  network_proxy_.reset(
      proxy_factory()->CreateModemGSMNetworkProxy(this,
                                                  cellular()->dbus_path(),
                                                  cellular()->dbus_owner()));
}

void CellularCapabilityGSM::UpdateStatus(const DBusPropertiesMap &properties) {
  string imsi;
  if (DBusProperties::GetString(properties, "imsi", &imsi)) {
    // TODO(petkov): Set GSM provider based on IMSI and SPN.
  }
}

void CellularCapabilityGSM::SetupConnectProperties(
    DBusPropertiesMap *properties) {
  (*properties)[Cellular::kConnectPropertyPhoneNumber].writer().append_string(
      kPhoneNumber);
  // TODO(petkov): Setup apn and "home_only".
}

void CellularCapabilityGSM::GetIdentifiers() {
  VLOG(2) << __func__;
  if (cellular()->imei().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_imei(card_proxy_->GetIMEI());
    VLOG(2) << "IMEI: " << cellular()->imei();
  }
  if (cellular()->imsi().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_imsi(card_proxy_->GetIMSI());
    VLOG(2) << "IMSI: " << cellular()->imsi();
  }
  if (spn_.empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    try {
      spn_ =  card_proxy_->GetSPN();
      VLOG(2) << "SPN: " << spn_;
    } catch (const DBus::Error e) {
      // Some modems don't support this call so catch the exception explicitly.
      LOG(WARNING) << "Unable to obtain SPN: " << e.what();
    }
  }
  if (cellular()->mdn().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_mdn(card_proxy_->GetMSISDN());
    VLOG(2) << "MSISDN/MDN: " << cellular()->mdn();
  }
}

void CellularCapabilityGSM::GetSignalQuality() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 strength = network_proxy_->GetSignalQuality();
  cellular()->HandleNewSignalQuality(strength);
}

void CellularCapabilityGSM::GetRegistrationState() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  ModemGSMNetworkProxyInterface::RegistrationInfo info =
      network_proxy_->GetRegistrationInfo();
  registration_state_ = info._1;
  network_id_ = info._2;
  operator_name_ = info._3;
  VLOG(2) << "GSM Registration: " << registration_state_ << ", "
          << network_id_ << ", "  << operator_name_;
  UpdateOperatorInfo();
  cellular()->HandleNewRegistrationState();
}

void CellularCapabilityGSM::GetProperties() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 tech = network_proxy_->AccessTechnology();
  SetAccessTechnology(tech);
  VLOG(2) << "GSM AccessTechnology: " << tech;
}

void CellularCapabilityGSM::UpdateOperatorInfo() {
  VLOG(2) << __func__;
  if (!network_id_.empty()) {
    VLOG(2) << "Looking up network id: " << network_id_;
    mobile_provider *provider =
        mobile_provider_lookup_by_network(cellular()->provider_db(),
                                          network_id_.c_str());
    if (provider) {
      const char *provider_name = mobile_provider_get_name(provider);
      if (provider_name && *provider_name) {
        operator_name_ = provider_name;
        operator_country_ = provider->country;
        VLOG(2) << "Operator name: " << operator_name_
                << ", country: " << operator_country_;
      }
    } else {
      VLOG(2) << "GSM provider not found.";
    }
  }
  UpdateServingOperator();
}

void CellularCapabilityGSM::UpdateServingOperator() {
  VLOG(2) << __func__;
  if (!cellular()->service().get()) {
    return;
  }
  Cellular::Operator oper;
  oper.SetName(operator_name_);
  oper.SetCode(network_id_);
  oper.SetCountry(operator_country_);
  cellular()->service()->set_serving_operator(oper);
}

void CellularCapabilityGSM::Register() {
  LOG(INFO) << __func__ << " \"" << selected_network_ << "\"";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  network_proxy_->Register(selected_network_);
  // TODO(petkov): Handle registration failure including trying the home network
  // when selected_network_ is not empty.
}

void CellularCapabilityGSM::RegisterOnNetwork(
    const string &network_id, Error */*error*/) {
  LOG(INFO) << __func__ << "(" << network_id << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::RegisterOnNetworkTask,
          network_id));
}

void CellularCapabilityGSM::RegisterOnNetworkTask(const string &network_id) {
  LOG(INFO) << __func__ << "(" << network_id << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  network_proxy_->Register(network_id);
  // TODO(petkov): Handle registration failure.
  selected_network_ = network_id;
}

void CellularCapabilityGSM::RequirePIN(
    const string &pin, bool require, Error */*error*/) {
  VLOG(2) << __func__ << "(" << pin << ", " << require << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::RequirePINTask, pin, require));
}

void CellularCapabilityGSM::RequirePINTask(const string &pin, bool require) {
  VLOG(2) << __func__ << "(" << pin << ", " << require << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->EnablePIN(pin, require);
}

void CellularCapabilityGSM::EnterPIN(const string &pin, Error */*error*/) {
  VLOG(2) << __func__ << "(" << pin << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::EnterPINTask, pin));
}

void CellularCapabilityGSM::EnterPINTask(const string &pin) {
  VLOG(2) << __func__ << "(" << pin << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->SendPIN(pin);
}

void CellularCapabilityGSM::UnblockPIN(
    const string &unblock_code, const string &pin, Error */*error*/) {
  VLOG(2) << __func__ << "(" << unblock_code << ", " << pin << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::UnblockPINTask, unblock_code, pin));
}

void CellularCapabilityGSM::UnblockPINTask(
    const string &unblock_code, const string &pin) {
  VLOG(2) << __func__ << "(" << unblock_code << ", " << pin << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->SendPUK(unblock_code, pin);
}

void CellularCapabilityGSM::ChangePIN(
    const string &old_pin, const string &new_pin, Error */*error*/) {
  VLOG(2) << __func__ << "(" << old_pin << ", " << new_pin << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::ChangePINTask, old_pin, new_pin));
}

void CellularCapabilityGSM::ChangePINTask(
    const string &old_pin, const string &new_pin) {
  VLOG(2) << __func__ << "(" << old_pin << ", " << new_pin << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->ChangePIN(old_pin, new_pin);
}

void CellularCapabilityGSM::Scan(Error */*error*/) {
  VLOG(2) << __func__;
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&CellularCapabilityGSM::ScanTask));
}

void CellularCapabilityGSM::ScanTask() {
  VLOG(2) << __func__;
  // TODO(petkov): Defer scan requests if a scan is in progress already.
  //
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583). This is a
  // must for this call which is basically a stub at this point.
  ModemGSMNetworkProxyInterface::ScanResults results = network_proxy_->Scan();
  found_networks_.clear();
  for (ModemGSMNetworkProxyInterface::ScanResults::const_iterator it =
           results.begin(); it != results.end(); ++it) {
    found_networks_.push_back(ParseScanResult(*it));
  }
}

Stringmap CellularCapabilityGSM::ParseScanResult(
    const ModemGSMNetworkProxyInterface::ScanResult &result) {
  Stringmap parsed;
  for (ModemGSMNetworkProxyInterface::ScanResult::const_iterator it =
           result.begin(); it != result.end(); ++it) {
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
    cellular()->service()->set_network_tech(GetNetworkTechnologyString());
  }
}

string CellularCapabilityGSM::GetNetworkTechnologyString() const {
  if (registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_HOME ||
      registration_state_ == MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING) {
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
        NOTREACHED();
    }
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
}

void CellularCapabilityGSM::OnServiceCreated() {
  cellular()->service()->set_activation_state(
      flimflam::kActivationStateActivated);
  UpdateServingOperator();
}

void CellularCapabilityGSM::OnGSMNetworkModeChanged(uint32 /*mode*/) {
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void CellularCapabilityGSM::OnGSMRegistrationInfoChanged(
    uint32 status, const string &operator_code, const string &operator_name) {
  registration_state_ = status;
  network_id_ = operator_code;
  operator_name_ = operator_name;
  UpdateOperatorInfo();
  cellular()->HandleNewRegistrationState();
}

void CellularCapabilityGSM::OnGSMSignalQualityChanged(uint32 quality) {
  cellular()->HandleNewSignalQuality(quality);
}

}  // namespace shill
