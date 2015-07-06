// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/schema_utils.h"

#include <algorithm>
#include <set>
#include <string>

#include <base/json/json_writer.h>

#include "libweave/src/commands/object_schema.h"
#include "libweave/src/commands/prop_types.h"
#include "libweave/src/commands/prop_values.h"

namespace buffet {
namespace {
// Helper function to report "type mismatch" errors when parsing JSON.
void ReportJsonTypeMismatch(const base::Value* value_in,
                            const std::string& expected_type,
                            chromeos::ErrorPtr* error) {
  std::string value_as_string;
  base::JSONWriter::Write(*value_in, &value_as_string);
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kTypeMismatch,
                               "Unable to convert value %s into %s",
                               value_as_string.c_str(), expected_type.c_str());
}

// Template version of ReportJsonTypeMismatch that deduces the type of expected
// data from the value_out parameter passed to particular overload of
// TypedValueFromJson() function. Always returns false.
template<typename T>
bool ReportUnexpectedJson(const base::Value* value_in, T*,
                          chromeos::ErrorPtr* error) {
  ReportJsonTypeMismatch(value_in,
                         PropType::GetTypeStringFromType(GetValueType<T>()),
                         error);
  return false;
}

bool ErrorMissingProperty(chromeos::ErrorPtr* error, const char* param_name) {
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kPropertyMissing,
                               "Required parameter missing: %s", param_name);
  return false;
}
}  // namespace

// Specializations of TypedValueToJson<T>() for supported C++ types.
std::unique_ptr<base::Value> TypedValueToJson(bool value,
                                              chromeos::ErrorPtr* error) {
  return std::unique_ptr<base::Value>(new base::FundamentalValue(value));
}

std::unique_ptr<base::Value> TypedValueToJson(int value,
                                              chromeos::ErrorPtr* error) {
  return std::unique_ptr<base::Value>(new base::FundamentalValue(value));
}

std::unique_ptr<base::Value> TypedValueToJson(double value,
                                              chromeos::ErrorPtr* error) {
  return std::unique_ptr<base::Value>(new base::FundamentalValue(value));
}

std::unique_ptr<base::Value> TypedValueToJson(const std::string& value,
                                              chromeos::ErrorPtr* error) {
  return std::unique_ptr<base::Value>(new base::StringValue(value));
}

std::unique_ptr<base::Value> TypedValueToJson(const native_types::Object& value,
                                              chromeos::ErrorPtr* error) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  for (const auto& pair : value) {
    auto prop_value = pair.second->ToJson(error);
    if (!prop_value)
      return prop_value;
    dict->SetWithoutPathExpansion(pair.first, prop_value.release());
  }
  return std::move(dict);
}

std::unique_ptr<base::Value> TypedValueToJson(const native_types::Array& value,
                                              chromeos::ErrorPtr* error) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (const auto& item : value) {
    auto json = item->ToJson(error);
    if (!json)
      return std::unique_ptr<base::Value>();
    list->Append(json.release());
  }
  return std::move(list);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        bool* value_out,
                        chromeos::ErrorPtr* error) {
  return value_in->GetAsBoolean(value_out) ||
         ReportUnexpectedJson(value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        int* value_out,
                        chromeos::ErrorPtr* error) {
  return value_in->GetAsInteger(value_out) ||
         ReportUnexpectedJson(value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        double* value_out,
                        chromeos::ErrorPtr* error) {
  return value_in->GetAsDouble(value_out) ||
         ReportUnexpectedJson(value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        std::string* value_out,
                        chromeos::ErrorPtr* error) {
  return value_in->GetAsString(value_out) ||
         ReportUnexpectedJson(value_in, value_out, error);
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        native_types::Object* value_out,
                        chromeos::ErrorPtr* error) {
  const base::DictionaryValue* dict = nullptr;
  if (!value_in->GetAsDictionary(&dict))
    return ReportUnexpectedJson(value_in, value_out, error);

  CHECK(type) << "Object definition must be provided";
  CHECK(ValueType::Object == type->GetType()) << "Type must be Object";

  const ObjectSchema* object_schema = type->GetObject()->GetObjectSchemaPtr();
  std::set<std::string> keys_processed;
  value_out->clear();  // Clear possible default values already in |value_out|.
  for (const auto& pair : object_schema->GetProps()) {
    const PropValue* def_value = pair.second->GetDefaultValue();
    if (dict->HasKey(pair.first)) {
      auto value = pair.second->CreateValue();
      const base::Value* param_value = nullptr;
      CHECK(dict->GetWithoutPathExpansion(pair.first, &param_value))
          << "Unable to get parameter";
      if (!value->FromJson(param_value, error) ||
          !pair.second->ValidateValue(value->GetValueAsAny(), error)) {
        chromeos::Error::AddToPrintf(error, FROM_HERE,
                                     errors::commands::kDomain,
                                     errors::commands::kInvalidPropValue,
                                     "Invalid value for property '%s'",
                                     pair.first.c_str());
        return false;
      }
      value_out->emplace_hint(value_out->end(), pair.first, std::move(value));
    } else if (def_value) {
      value_out->emplace_hint(value_out->end(), pair.first, def_value->Clone());
    } else {
      return ErrorMissingProperty(error, pair.first.c_str());
    }
    keys_processed.insert(pair.first);
  }

  // Just for sanity, make sure that we processed all the necessary properties
  // and there weren't any extra (unknown) ones specified. If so, ignore
  // them, but log as warnings...
  base::DictionaryValue::Iterator iter(*dict);
  while (!iter.IsAtEnd()) {
    std::string key = iter.key();
    if (keys_processed.find(key) == keys_processed.end() &&
        !object_schema->GetExtraPropertiesAllowed()) {
      chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
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
      chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                   errors::commands::kInvalidPropValue,
                                   "Invalid value for property '%s'",
                                   pair.first.c_str());
      return false;
    }
  }
  return true;
}

bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        native_types::Array* value_out,
                        chromeos::ErrorPtr* error) {
  const base::ListValue* list = nullptr;
  if (!value_in->GetAsList(&list))
    return ReportUnexpectedJson(value_in, value_out, error);

  CHECK(type) << "Array type definition must be provided";
  CHECK(ValueType::Array == type->GetType()) << "Type must be Array";
  const PropType* item_type = type->GetArray()->GetItemTypePtr();
  CHECK(item_type) << "Incomplete array type definition";

  // This value might already contain values from the type defaults.
  // Clear them first before proceeding.
  value_out->clear();
  value_out->reserve(list->GetSize());
  for (const base::Value* item : *list) {
    std::unique_ptr<PropValue> prop_value = item_type->CreateValue();
    if (!prop_value->FromJson(item, error) ||
        !item_type->ValidateValue(prop_value->GetValueAsAny(), error)) {
      return false;
    }
    value_out->push_back(std::move(prop_value));
  }
  return true;
}

// Compares two sets of key-value pairs from two Objects.
static bool obj_cmp(const native_types::Object::value_type& v1,
                    const native_types::Object::value_type& v2) {
  return (v1.first == v2.first) && v1.second->IsEqual(v2.second.get());
}

bool operator==(const native_types::Object& obj1,
                const native_types::Object& obj2) {
  if (obj1.size() != obj2.size())
    return false;

  auto pair = std::mismatch(obj1.begin(), obj1.end(), obj2.begin(), obj_cmp);
  return pair == std::make_pair(obj1.end(), obj2.end());
}

bool operator==(const native_types::Array& arr1,
                const native_types::Array& arr2) {
  if (arr1.size() != arr2.size())
    return false;

  using Type = const native_types::Array::value_type;
  // Compare two array items.
  auto arr_cmp = [](const Type& v1, const Type& v2) {
    return v1->IsEqual(v2.get());
  };
  auto pair = std::mismatch(arr1.begin(), arr1.end(), arr2.begin(), arr_cmp);
  return pair == std::make_pair(arr1.end(), arr2.end());
}

std::string ToString(const native_types::Object& obj) {
  auto val = TypedValueToJson(obj, nullptr);
  std::string str;
  base::JSONWriter::Write(*val, &str);
  return str;
}

std::string ToString(const native_types::Array& arr) {
  auto val = TypedValueToJson(arr, nullptr);
  std::string str;
  base::JSONWriter::Write(*val, &str);
  return str;
}

chromeos::Any PropValueToDBusVariant(const PropValue* value) {
  if (value->GetType() == ValueType::Object)
    return ObjectToDBusVariant(value->GetObject()->GetValue());

  if (value->GetType() == ValueType::Array) {
    const PropType* item_type =
        value->GetPropType()->GetArray()->GetItemTypePtr();
    return item_type->ConvertArrayToDBusVariant(value->GetArray()->GetValue());
  }
  return value->GetValueAsAny();
}

chromeos::VariantDictionary
ObjectToDBusVariant(const native_types::Object& object) {
  chromeos::VariantDictionary dict;
  for (const auto& pair : object) {
    // Since we are inserting the elements from native_types::Object which is
    // a map, the keys are already sorted. So use the "end()" position as a hint
    // for dict.insert() so the destination map can optimize its insertion
    // time.
    chromeos::Any prop = PropValueToDBusVariant(pair.second.get());
    dict.emplace_hint(dict.end(), pair.first, std::move(prop));
  }
  return dict;
}

std::unique_ptr<const PropValue> PropValueFromDBusVariant(
    const PropType* type,
    const chromeos::Any& value,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<const PropValue> result;
  if (type->GetType() == ValueType::Array) {
    // Special case for array types.
    // We expect the |value| to contain std::vector<T>, while PropValue must use
    // native_types::Array instead. Do the conversion.
    native_types::Array arr;
    const PropType* item_type = type->GetArray()->GetItemTypePtr();
    if (item_type->ConvertDBusVariantToArray(value, &arr, error))
      result = type->CreateValue(arr, error);
  } else if (type->GetType() == ValueType::Object) {
    // Special case for object types.
    // We expect the |value| to contain chromeos::VariantDictionary, while
    // PropValue must use native_types::Object instead. Do the conversion.
    if (!value.IsTypeCompatible<chromeos::VariantDictionary>()) {
      type->GenerateErrorValueTypeMismatch(error);
      return result;
    }
    CHECK(nullptr != type->GetObject()->GetObjectSchemaPtr())
        << "An object type must have a schema defined for it";
    native_types::Object obj;
    if (!ObjectFromDBusVariant(type->GetObject()->GetObjectSchemaPtr(),
                               value.Get<chromeos::VariantDictionary>(),
                               &obj,
                               error)) {
      return result;
    }

    result = type->CreateValue(std::move(obj), error);
  } else {
    result = type->CreateValue(value, error);
  }

  return result;
}

bool ObjectFromDBusVariant(const ObjectSchema* object_schema,
                           const chromeos::VariantDictionary& dict,
                           native_types::Object* obj,
                           chromeos::ErrorPtr* error) {
  std::set<std::string> keys_processed;
  obj->clear();
  // First go over all object parameters defined by type's object schema and
  // extract the corresponding parameters from the source dictionary.
  for (const auto& pair : object_schema->GetProps()) {
    const PropValue* def_value = pair.second->GetDefaultValue();
    auto it = dict.find(pair.first);
    if (it != dict.end()) {
      const PropType* prop_type = pair.second.get();
      CHECK(prop_type) << "Value property type must be available";
      auto prop_value = PropValueFromDBusVariant(prop_type, it->second, error);
      if (!prop_value) {
        chromeos::Error::AddToPrintf(error, FROM_HERE,
                                     errors::commands::kDomain,
                                     errors::commands::kInvalidPropValue,
                                     "Invalid value for property '%s'",
                                     pair.first.c_str());
        return false;
      }
      obj->emplace_hint(obj->end(), pair.first, std::move(prop_value));
    } else if (def_value) {
      obj->emplace_hint(obj->end(), pair.first, def_value->Clone());
    } else {
      ErrorMissingProperty(error, pair.first.c_str());
      return false;
    }
    keys_processed.insert(pair.first);
  }

  // Make sure that we processed all the necessary properties and there weren't
  // any extra (unknown) ones specified, unless the schema allows them.
  if (!object_schema->GetExtraPropertiesAllowed()) {
    for (const auto& pair : dict) {
      if (keys_processed.find(pair.first) == keys_processed.end()) {
        chromeos::Error::AddToPrintf(error, FROM_HERE,
                                     errors::commands::kDomain,
                                     errors::commands::kUnknownProperty,
                                     "Unrecognized property '%s'",
                                     pair.first.c_str());
        return false;
      }
    }
  }

  return true;
}

}  // namespace buffet
