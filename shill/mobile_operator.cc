// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mobile_operator.h"

#include <vector>

#include <base/stl_util.h>
#include <chromeos/dbus/service_constants.h>
#include <mobile_provider.h>

#include "shill/cellular_operator_info.h"
#include "shill/logging.h"
#include "shill/modem_info.h"

using std::string;
using std::vector;

namespace shill {

namespace {

const char kCodeKey[] = "code";
const char kCountryKey[] = "country";
const char kNameKey[] = "name";

bool CompareStringmapValue(const Stringmap &dict, const string &key,
                           const string &value) {
  if (!ContainsKey(dict, key))
    return value.empty();
  return dict.find(key)->second == value;
}

const CellularOperatorInfo::CellularOperator *LookupCellularOperatorInfo(
    shill::ModemInfo *modem_info, const string &code,
    MobileOperator::OperatorCodeType type) {
  if (type == MobileOperator::kOperatorCodeTypeMCCMNC)
    return modem_info->cellular_operator_info()->
        GetCellularOperatorByMCCMNC(code);
  if (type == MobileOperator::kOperatorCodeTypeSID)
    return modem_info->cellular_operator_info()->
        GetCellularOperatorBySID(code);
  return nullptr;
}

mobile_provider *LookupMobileProviderDB(
    shill::ModemInfo *modem_info, const string &code, const string &name,
    MobileOperator::OperatorCodeType type) {
  if (type != MobileOperator::kOperatorCodeTypeMCCMNC)
    return nullptr;
  return mobile_provider_lookup_best_match(
      modem_info->provider_db(), name.c_str(), code.c_str());
}

bool AssignDictData(const Stringmap &from, const vector<string> &keys,
                    Stringmap *to) {
  bool contents_changed = false;
  for (const string &key : keys) {
    string value;
    if (ContainsKey(from, key))
      value = from.find(key)->second;
    if (!CompareStringmapValue(*to, key, value)) {
      contents_changed = true;
      if (value.empty())
        to->erase(key);
      else
        (*to)[key] = value;
    }
  }
  return contents_changed;
}

bool AssignOperatorData(const Stringmap &from, Stringmap *to) {
  vector<string> keys = { kCodeKey, kNameKey, kCountryKey };
  return AssignDictData(from, keys, to);
}

bool AssignOlpData(const Stringmap &from, Stringmap *to) {
  vector<string> keys = { "url", "method", "postdata" };
  return AssignDictData(from, keys, to);
}

Stringmaps BuildApnListFromCellularOperatorInfoResult(
    const CellularOperatorInfo::CellularOperator *info) {
  Stringmaps apn_list;
  for (const CellularOperatorInfo::MobileAPN *apn : info->apn_list().get()) {
    Stringmap apn_dict;
    if (!apn->apn.empty())
      apn_dict[shill::kApnProperty] = apn->apn;
    if (!apn->username.empty())
      apn_dict[shill::kApnUsernameProperty] = apn->username;
    if (!apn->password.empty())
      apn_dict[shill::kApnPasswordProperty] = apn->password;
    string lname, lang, name;
    for (const CellularOperatorInfo::LocalizedName &apn_name :
            apn->name_list) {
      if (!apn_name.language.empty()) {
        if (lname.empty()) {
          lname = apn_name.name;
          lang = apn_name.language;
        }
      } else if (name.empty()) {
        name = apn_name.name;
      }
    }
    if (!name.empty())
      apn_dict[shill::kApnNameProperty] = name;
    if (!lname.empty()) {
      apn_dict[shill::kApnLocalizedNameProperty] = lname;
      apn_dict[shill::kApnLanguageProperty] = lang;
    }
    apn_list.push_back(apn_dict);
  }
  return apn_list;
}

Stringmaps BuildApnListFromMobileProviderDbResult(mobile_provider *provider) {
  Stringmaps apn_list;
  for (int i = 0; i < provider->num_apns; ++i) {
    Stringmap apn_dict;
    mobile_apn *apn = provider->apns[i];
    if (apn->value)
      apn_dict[shill::kApnProperty] = apn->value;
    if (apn->username)
      apn_dict[shill::kApnUsernameProperty] = apn->username;
    if (apn->password)
      apn_dict[shill::kApnPasswordProperty] = apn->password;
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
      apn_dict[kApnNameProperty] = name->name;
    }
    if (lname) {
      apn_dict[kApnLocalizedNameProperty] = lname->name;
      apn_dict[kApnLanguageProperty] = lname->lang;
    }
    apn_list.push_back(apn_dict);
  }
  return apn_list;
}

struct LookupResult {
  // |operator_data| contains the following keys:
  //
  //    kNameKey: The name of the operator,
  //    kCodeKey: The operator code,
  //    kCountryKey: Country code,
  //
  // Any of the above keys may not be present in the returned dictionary, if a
  // matching value was not found.
  Stringmap operator_data;
  bool requires_roaming;
  Stringmaps apn_list;
};

// Finds the best match for the given data based on CellularOperatorInfo and
// mobile_provider_db. If |get_apns| is true, an APN list will be constructed
// if a carrier is found. If no match is found in the database, the kNameKey
// and kCodeKey keys for |operator_data| of the return value will be set to
// |operator_name| and |operator_code|, respectively.
LookupResult FindMatchingOperatorResult(shill::ModemInfo *modem_info,
                                        const string &operator_code,
                                        const string &operator_name,
                                        MobileOperator::OperatorCodeType type,
                                        bool get_apns) {
  LookupResult result;
  // First look up in CellularOperatorInfo.
  const CellularOperatorInfo::CellularOperator *info =
      LookupCellularOperatorInfo(modem_info, operator_code, type);
  if (info) {
    // Match found, depend on information from here.
    SLOG(Cellular, 3) << "Found match for operator code " << operator_code
                      << "in CellularOperatorInfo.";
    result.operator_data[kCodeKey] = operator_code;
    if (!operator_name.empty())
      result.operator_data[kNameKey] = operator_name;
    else if (!info->name_list().empty())
      result.operator_data[kNameKey] = info->name_list()[0].name;
    result.operator_data[kCountryKey] = info->country();
    result.requires_roaming = false;

    // Build APN list if requested.
    if (get_apns)
      result.apn_list = BuildApnListFromCellularOperatorInfoResult(info);
    return result;
  }
  // Look up mobile_provider_db.
  mobile_provider *provider = LookupMobileProviderDB(
      modem_info, operator_code, operator_name, type);
  if (provider) {
    SLOG(Cellular, 3) << "Found match for operator code " << operator_code
                      << "in mobile_provider_db.";
    // mobile_provider_db look up works using both |code| and |name|. If no
    // operator code was provided, use what was returned from the database.
    if (!operator_code.empty())
      result.operator_data[kCodeKey] = operator_code;
    else if (provider->networks && provider->networks[0])
      result.operator_data[kCodeKey] = provider->networks[0];
    if (provider->country)
      result.operator_data[kCountryKey] = provider->country;
    if (!operator_name.empty())
      result.operator_data[kNameKey] = operator_name;
    else {
      const gchar *name = mobile_provider_get_name(provider);
      if (name)
        result.operator_data[kNameKey] = name;
    }
    result.requires_roaming = provider->requires_roaming;

    // Build APN list if requested.
    if (get_apns)
      result.apn_list = BuildApnListFromMobileProviderDbResult(provider);
    return result;
  }
  SLOG(Cellular, 3) << "No match found for operator code "
                    << operator_code << ".";
  if (!operator_code.empty())
    result.operator_data[kCodeKey] = operator_code;
  if (!operator_name.empty())
    result.operator_data[kNameKey] = operator_name;
  return result;
}

}  // namespace

MobileOperator::MobileOperator(ModemInfo *modem_info)
    : modem_info_(modem_info),
      home_provider_requires_roaming_(false) {}

void MobileOperator::AddObserver(Observer *observer) {
  observers_.AddObserver(observer);
}

void MobileOperator::RemoveObserver(Observer *observer) {
  observers_.RemoveObserver(observer);
}

void MobileOperator::SimOperatorInfoReceived(const string &operator_code,
                                             const string &operator_name) {
  if (operator_code.empty() && operator_name.empty()) {
    // Clear the home provider.
    if (!home_provider_.empty()) {
      home_provider_.clear();
      NotifyHomeProviderInfoChanged();
    }
    return;
  }
  LookupResult result = FindMatchingOperatorResult(
      modem_info_, operator_code, operator_name,
      kOperatorCodeTypeMCCMNC, false);
  home_provider_requires_roaming_ = result.requires_roaming;
  if (AssignOperatorData(result.operator_data, &home_provider_))
    NotifyHomeProviderInfoChanged();
}

void MobileOperator::OtaOperatorInfoReceived(const string &operator_code,
                                             const string &operator_name,
                                             OperatorCodeType type) {
  if (operator_code.empty() && operator_name.empty()) {
    apn_list_.clear();
    online_payment_url_template_.clear();
    if (!serving_operator_.empty()) {
      serving_operator_.clear();
      NotifyServingOperatorInfoChanged();
    }
    return;
  }

  // Look up the operator and assign it to |serving_operator_|.
  LookupResult result = FindMatchingOperatorResult(
      modem_info_, operator_code, operator_name, type, true);
  bool serving_operator_changed =
      AssignOperatorData(result.operator_data, &serving_operator_);

  // For now, always notify that APNList changed, as long as the constructed
  // list is non-empty.
  if (!apn_list_.empty() || !result.apn_list.empty()) {
    apn_list_ = result.apn_list;
    NotifyApnListChanged();
  }

  // Update the OLP. Notify observers if OLP changed.
  const CellularService::OLP *olp_result = NULL;
  if (type == kOperatorCodeTypeMCCMNC) {
    olp_result = modem_info_->cellular_operator_info()->
        GetOLPByMCCMNC(operator_code);
  } else {
    olp_result = modem_info_->cellular_operator_info()->
        GetOLPBySID(operator_code);
  }
  if (olp_result) {
    Stringmap olp_dict = olp_result->ToDict();
    if (AssignOlpData(olp_dict, &online_payment_url_template_))
      NotifyOnlinePaymentUrlTemplateChanged();
  } else if (!online_payment_url_template_.empty()) {
    online_payment_url_template_.clear();
    NotifyOnlinePaymentUrlTemplateChanged();
  }
  if (serving_operator_changed)
    NotifyServingOperatorInfoChanged();
}

void MobileOperator::NotifyHomeProviderInfoChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnHomeProviderInfoChanged(this));
}

void MobileOperator::NotifyServingOperatorInfoChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnServingOperatorInfoChanged(this));
}

void MobileOperator::NotifyApnListChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnApnListChanged(this));
}

void MobileOperator::NotifyOnlinePaymentUrlTemplateChanged() {
  FOR_EACH_OBSERVER(
      Observer, observers_, OnOnlinePaymentUrlTemplateChanged(this));
}

}  // namespace shill
