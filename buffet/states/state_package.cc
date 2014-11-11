// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/states/state_package.h"

#include <base/logging.h>
#include <base/values.h>
#include <chromeos/dbus/data_serialization.h>

#include "buffet/commands/prop_types.h"
#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_utils.h"
#include "buffet/states/error_codes.h"

namespace buffet {

StatePackage::StatePackage(const std::string& name) : name_(name) {
}

bool StatePackage::AddSchemaFromJson(const base::DictionaryValue* json,
                                     chromeos::ErrorPtr* error) {
  ObjectSchema schema;
  if (!schema.FromJson(json, nullptr, error))
    return false;

  // Scan first to make sure we have no property redefinitions.
  for (const auto& pair : schema.GetProps()) {
    if (types_.GetProp(pair.first)) {
      chromeos::Error::AddToPrintf(error, FROM_HERE, errors::state::kDomain,
                                    errors::state::kPropertyRedefinition,
                                    "State property '%s.%s' is already defined",
                                    name_.c_str(), pair.first.c_str());
      return false;
    }
  }

  // Now move all the properties to |types_| object.
  for (const auto& pair : schema.GetProps()) {
    types_.AddProp(pair.first, pair.second);
    // Create default value for this state property.
    values_.insert(std::make_pair(pair.first, pair.second->CreateValue()));
  }

  return true;
}

bool StatePackage::AddValuesFromJson(const base::DictionaryValue* json,
                                     chromeos::ErrorPtr* error) {
  base::DictionaryValue::Iterator iter(*json);
  while (!iter.IsAtEnd()) {
    std::string property_name = iter.key();
    auto it = values_.find(property_name);
    if (it == values_.end()) {
      chromeos::Error::AddToPrintf(error, FROM_HERE, errors::state::kDomain,
                                    errors::state::kPropertyNotDefined,
                                    "State property '%s.%s' is not defined",
                                    name_.c_str(), property_name.c_str());
      return false;
    }
    auto new_value = it->second->GetPropType()->CreateValue();
    if (!new_value->FromJson(&iter.value(), error))
      return false;
    it->second = new_value;
    iter.Advance();
  }
  return true;
}

std::unique_ptr<base::DictionaryValue> StatePackage::GetValuesAsJson(
      chromeos::ErrorPtr* error) const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  for (const auto& pair : values_) {
    auto value = pair.second->ToJson(error);
    if (!value) {
      dict.reset();
      break;
    }
    dict->SetWithoutPathExpansion(pair.first, value.release());
  }
  return dict;
}

chromeos::Any StatePackage::GetPropertyValue(const std::string& property_name,
                                             chromeos::ErrorPtr* error) const {
  auto it = values_.find(property_name);
  if (it == values_.end()) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::state::kDomain,
                                  errors::state::kPropertyNotDefined,
                                  "State property '%s.%s' is not defined",
                                  name_.c_str(), property_name.c_str());
    return chromeos::Any();
  }
  return PropValueToDBusVariant(it->second.get());
}

bool StatePackage::SetPropertyValue(const std::string& property_name,
                                    const chromeos::Any& value,
                                    chromeos::ErrorPtr* error) {
  auto it = values_.find(property_name);
  if (it == values_.end()) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::state::kDomain,
                                  errors::state::kPropertyNotDefined,
                                  "State property '%s.%s' is not defined",
                                  name_.c_str(), property_name.c_str());
    return false;
  }
  auto new_value = PropValueFromDBusVariant(it->second->GetPropType(),
                                            value, error);
  if (!new_value)
    return false;
  it->second = new_value;
  return true;
}

}  // namespace buffet
