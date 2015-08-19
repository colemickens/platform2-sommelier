// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/schema_utils.h"

#include <algorithm>
#include <set>
#include <string>

#include <base/json/json_writer.h>
#include <base/logging.h>

#include "libweave/src/commands/object_schema.h"
#include "libweave/src/commands/prop_types.h"
#include "libweave/src/commands/prop_values.h"

namespace weave {
namespace {
// Helper function to report "type mismatch" errors when parsing JSON.
void ReportJsonTypeMismatch(const tracked_objects::Location& location,
                            const base::Value* value_in,
                            const std::string& expected_type,
                            ErrorPtr* error) {
  std::string value_as_string;
  base::JSONWriter::Write(*value_in, &value_as_string);
  Error::AddToPrintf(error, location, errors::commands::kDomain,
                     errors::commands::kTypeMismatch,
                     "Unable to convert value %s into %s",
                     value_as_string.c_str(), expected_type.c_str());
}

// Template version of ReportJsonTypeMismatch that deduces the type of expected
// data from the value_out parameter passed to particular overload of
// TypedValueFromJson() function. Always returns false.
template <typename T>
bool ReportUnexpectedJson(const tracked_objects::Location& location,
                          const base::Value* value_in,
                          T*,
                          ErrorPtr* error) {
  ReportJsonTypeMismatch(location, value_in,
                         PropType::GetTypeStringFromType(GetValueType<T>()),
                         error);
  return false;
}

bool ErrorMissingProperty(ErrorPtr* error,
                          const tracked_objects::Location& location,
                          const char* param_name) {
  Error::AddToPrintf(error, location, errors::commands::kDomain,
                     errors::commands::kPropertyMissing,
                     "Required parameter missing: %s", param_name);
  return false;
}

}  // namespace

// Specializations of TypedValueToJson<T>() for supported C++ types.
std::unique_ptr<base::FundamentalValue> TypedValueToJson(bool value) {
  return std::unique_ptr<base::FundamentalValue>(
      new base::FundamentalValue(value));
}

std::unique_ptr<base::FundamentalValue> TypedValueToJson(int value) {
  return std::unique_ptr<base::FundamentalValue>(
      new base::FundamentalValue(value));
}

std::unique_ptr<base::FundamentalValue> TypedValueToJson(double value) {
  return std::unique_ptr<base::FundamentalValue>(
      new base::FundamentalValue(value));
}

std::unique_ptr<base::StringValue> TypedValueToJson(const std::string& value) {
  return std::unique_ptr<base::StringValue>(new base::StringValue(value));
}

std::unique_ptr<base::DictionaryValue> TypedValueToJson(const ValueMap& value) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  for (const auto& pair : value) {
    auto prop_value = pair.second->ToJson();
    CHECK(prop_value);
    dict->SetWithoutPathExpansion(pair.first, prop_value.release());
  }
  return dict;
}

std::unique_ptr<base::ListValue> TypedValueToJson(const ValueVector& value) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (const auto& item : value) {
    auto json = item->ToJson();
    CHECK(json);
    list->Append(json.release());
  }
  return list;
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        bool* value_out,
                        ErrorPtr* error) {
  return value_in->GetAsBoolean(value_out) ||
         ReportUnexpectedJson(FROM_HERE, value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        int* value_out,
                        ErrorPtr* error) {
  return value_in->GetAsInteger(value_out) ||
         ReportUnexpectedJson(FROM_HERE, value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        double* value_out,
                        ErrorPtr* error) {
  return value_in->GetAsDouble(value_out) ||
         ReportUnexpectedJson(FROM_HERE, value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        std::string* value_out,
                        ErrorPtr* error) {
  return value_in->GetAsString(value_out) ||
         ReportUnexpectedJson(FROM_HERE, value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        ValueMap* value_out,
                        ErrorPtr* error) {
  const base::DictionaryValue* dict = nullptr;
  if (!value_in->GetAsDictionary(&dict))
    return ReportUnexpectedJson(FROM_HERE, value_in, value_out, error);

  CHECK(type) << "Object definition must be provided";
  CHECK(ValueType::Object == type->GetType()) << "Type must be Object";

  const ObjectSchema* object_schema = type->GetObject()->GetObjectSchemaPtr();
  std::set<std::string> keys_processed;
  value_out->clear();  // Clear possible default values already in |value_out|.
  for (const auto& pair : object_schema->GetProps()) {
    const PropValue* def_value = pair.second->GetDefaultValue();
    if (dict->HasKey(pair.first)) {
      const base::Value* param_value = nullptr;
      CHECK(dict->GetWithoutPathExpansion(pair.first, &param_value))
          << "Unable to get parameter";
      auto value = pair.second->CreatePropValue(*param_value, error);
      if (!value) {
        Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                           errors::commands::kInvalidPropValue,
                           "Invalid value for property '%s'",
                           pair.first.c_str());
        return false;
      }
      value_out->emplace_hint(value_out->end(), pair.first, std::move(value));
      keys_processed.insert(pair.first);
    } else if (def_value) {
      value_out->emplace_hint(value_out->end(), pair.first, def_value->Clone());
      keys_processed.insert(pair.first);
    } else if (pair.second->IsRequired()) {
      return ErrorMissingProperty(error, FROM_HERE, pair.first.c_str());
    }
  }

  // Just for sanity, make sure that we processed all the necessary properties
  // and there weren't any extra (unknown) ones specified. If so, ignore
  // them, but log as warnings...
  base::DictionaryValue::Iterator iter(*dict);
  while (!iter.IsAtEnd()) {
    std::string key = iter.key();
    if (keys_processed.find(key) == keys_processed.end() &&
        !object_schema->GetExtraPropertiesAllowed()) {
      Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                         errors::commands::kUnknownProperty,
                         "Unrecognized parameter '%s'", key.c_str());
      return false;
    }
    iter.Advance();
  }

  // Now go over all property values and validate them.
  for (const auto& pair : *value_out) {
    const PropType* prop_type = pair.second->GetPropType();
    CHECK(prop_type) << "Value property type must be available";
    if (!prop_type->ValidateConstraints(*pair.second, error)) {
      Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                         errors::commands::kInvalidPropValue,
                         "Invalid value for property '%s'", pair.first.c_str());
      return false;
    }
  }
  return true;
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        ValueVector* value_out,
                        ErrorPtr* error) {
  const base::ListValue* list = nullptr;
  if (!value_in->GetAsList(&list))
    return ReportUnexpectedJson(FROM_HERE, value_in, value_out, error);

  CHECK(type) << "Array type definition must be provided";
  CHECK(ValueType::Array == type->GetType()) << "Type must be Array";
  const PropType* item_type = type->GetArray()->GetItemTypePtr();
  CHECK(item_type) << "Incomplete array type definition";

  // This value might already contain values from the type defaults.
  // Clear them first before proceeding.
  value_out->clear();
  value_out->reserve(list->GetSize());
  for (const base::Value* item : *list) {
    std::unique_ptr<PropValue> prop_value =
        item_type->CreatePropValue(*item, error);
    if (!prop_value)
      return false;
    value_out->push_back(std::move(prop_value));
  }
  return true;
}

// Compares two sets of key-value pairs from two Objects.
static bool obj_cmp(const ValueMap::value_type& v1,
                    const ValueMap::value_type& v2) {
  return (v1.first == v2.first) && v1.second->IsEqual(v2.second.get());
}

bool operator==(const ValueMap& obj1, const ValueMap& obj2) {
  if (obj1.size() != obj2.size())
    return false;

  auto pair = std::mismatch(obj1.begin(), obj1.end(), obj2.begin(), obj_cmp);
  return pair == std::make_pair(obj1.end(), obj2.end());
}

bool operator==(const ValueVector& arr1, const ValueVector& arr2) {
  if (arr1.size() != arr2.size())
    return false;

  using Type = const ValueVector::value_type;
  // Compare two array items.
  auto arr_cmp = [](const Type& v1, const Type& v2) {
    return v1->IsEqual(v2.get());
  };
  auto pair = std::mismatch(arr1.begin(), arr1.end(), arr2.begin(), arr_cmp);
  return pair == std::make_pair(arr1.end(), arr2.end());
}

std::string ToString(const ValueMap& obj) {
  auto val = TypedValueToJson(obj);
  std::string str;
  base::JSONWriter::Write(*val, &str);
  return str;
}

std::string ToString(const ValueVector& arr) {
  auto val = TypedValueToJson(arr);
  std::string str;
  base::JSONWriter::Write(*val, &str);
  return str;
}

}  // namespace weave
