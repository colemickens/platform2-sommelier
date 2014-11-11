// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/object_schema.h"

#include <algorithm>
#include <limits>

#include <base/logging.h>
#include <base/values.h>

#include "buffet/commands/prop_types.h"
#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_constants.h"

namespace buffet {

void ObjectSchema::AddProp(const std::string& name,
                           std::shared_ptr<PropType> prop) {
  properties_[name] = prop;
}

const PropType* ObjectSchema::GetProp(const std::string& name) const {
  auto p = properties_.find(name);
  return p != properties_.end() ? p->second.get() : nullptr;
}

std::unique_ptr<base::DictionaryValue> ObjectSchema::ToJson(
    bool full_schema, chromeos::ErrorPtr* error) const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  for (const auto& pair : properties_) {
    auto PropDef = pair.second->ToJson(full_schema, error);
    if (!PropDef)
      return std::unique_ptr<base::DictionaryValue>();
    value->SetWithoutPathExpansion(pair.first, PropDef.release());
  }
  return value;
}

bool ObjectSchema::FromJson(const base::DictionaryValue* value,
                            const ObjectSchema* object_schema,
                            chromeos::ErrorPtr* error) {
  Properties properties;
  base::DictionaryValue::Iterator iter(*value);
  while (!iter.IsAtEnd()) {
    std::string name = iter.key();
    const PropType* base_schema =
        object_schema ? object_schema->GetProp(iter.key()) : nullptr;
    if (!PropFromJson(iter.key(), iter.value(), base_schema, &properties,
                      error))
      return false;
    iter.Advance();
  }
  properties_ = std::move(properties);
  return true;
}

static std::unique_ptr<PropType> CreatePropType(const std::string& type_name,
                                                const std::string& prop_name,
                                                chromeos::ErrorPtr* error) {
  std::unique_ptr<PropType> prop;
  ValueType type;
  if (PropType::GetTypeFromTypeString(type_name, &type))
    prop = PropType::Create(type);
  if (!prop) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kUnknownType,
                                 "Unknown type %s for parameter %s",
                                 type_name.c_str(), prop_name.c_str());
  }
  return prop;
}

static bool ErrorInvalidTypeInfo(const std::string& prop_name,
                                 chromeos::ErrorPtr* error) {
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kNoTypeInfo,
                               "Unable to determine parameter type for %s",
                               prop_name.c_str());
  return false;
}

bool ObjectSchema::PropFromJson(const std::string& prop_name,
                                const base::Value& value,
                                const PropType* base_schema,
                                Properties* properties,
                                chromeos::ErrorPtr* error) const {
  if (value.IsType(base::Value::TYPE_STRING)) {
    // A string value is a short-hand object specification and provides
    // the parameter type.
    return PropFromJsonString(prop_name, value, base_schema, properties,
                              error);
  } else if (value.IsType(base::Value::TYPE_LIST)) {
    // One of the enumerated types.
    return PropFromJsonArray(prop_name, value, base_schema, properties,
                             error);
  } else if (value.IsType(base::Value::TYPE_DICTIONARY)) {
    // Full parameter definition.
    return PropFromJsonObject(prop_name, value, base_schema, properties,
                              error);
  }
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kInvalidPropDef,
                               "Invalid parameter definition for %s",
                               prop_name.c_str());
  return false;
}

bool ObjectSchema::PropFromJsonString(const std::string& prop_name,
                                      const base::Value& value,
                                      const PropType* base_schema,
                                      Properties* properties,
                                      chromeos::ErrorPtr* error) const {
  std::string type_name;
  CHECK(value.GetAsString(&type_name)) << "Unable to get string value";
  std::unique_ptr<PropType> prop = CreatePropType(type_name, prop_name, error);
  if (!prop)
    return false;
  base::DictionaryValue empty;
  if (!prop->FromJson(&empty, base_schema, error)) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kInvalidPropDef,
                                 "Error in definition of property '%s'",
                                 prop_name.c_str());
    return false;
  }
  properties->insert(std::make_pair(prop_name, std::move(prop)));
  return true;
}

static std::string DetectArrayType(const base::ListValue* list,
                                   const PropType* base_schema) {
  std::string type_name;
  if (base_schema) {
    type_name = base_schema->GetTypeAsString();
  } else if (list->GetSize() > 0) {
    const base::Value* first_element = nullptr;
    if (list->Get(0, &first_element)) {
      switch (first_element->GetType()) {
      case base::Value::TYPE_BOOLEAN:
        type_name = PropType::GetTypeStringFromType(ValueType::Boolean);
        break;
      case base::Value::TYPE_INTEGER:
        type_name = PropType::GetTypeStringFromType(ValueType::Int);
        break;
      case base::Value::TYPE_DOUBLE:
        type_name = PropType::GetTypeStringFromType(ValueType::Double);
        break;
      case base::Value::TYPE_STRING:
        type_name = PropType::GetTypeStringFromType(ValueType::String);
        break;
      case base::Value::TYPE_DICTIONARY:
        type_name = PropType::GetTypeStringFromType(ValueType::Object);
        break;
      default:
        // The rest are unsupported.
        break;
      }
    }
  }
  return type_name;
}

bool ObjectSchema::PropFromJsonArray(const std::string& prop_name,
                                     const base::Value& value,
                                     const PropType* base_schema,
                                     Properties* properties,
                                     chromeos::ErrorPtr* error) const {
  const base::ListValue* list = nullptr;
  CHECK(value.GetAsList(&list)) << "Unable to get array value";
  std::string type_name = DetectArrayType(list, base_schema);
  if (type_name.empty())
    return ErrorInvalidTypeInfo(prop_name, error);
  std::unique_ptr<PropType> prop = CreatePropType(type_name, prop_name, error);
  if (!prop)
    return false;
  base::DictionaryValue array_object;
  array_object.SetWithoutPathExpansion(commands::attributes::kOneOf_Enum,
                                       list->DeepCopy());
  if (!prop->FromJson(&array_object, base_schema, error)) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kInvalidPropDef,
                                 "Error in definition of property '%s'",
                                 prop_name.c_str());
    return false;
  }
  properties->insert(std::make_pair(prop_name, std::move(prop)));
  return true;
}

static std::string DetectObjectType(const base::DictionaryValue* dict,
                                    const PropType* base_schema) {
  bool has_min_max = dict->HasKey(commands::attributes::kNumeric_Min) ||
                     dict->HasKey(commands::attributes::kNumeric_Max);

  // Here we are trying to "detect the type and read in the object based on
  // the deduced type". Later, we'll verify that this detected type matches
  // the expectation of the base schema, if applicable, to make sure we are not
  // changing the expected type. This makes the vendor-side (re)definition of
  // standard and custom commands behave exactly the same.
  // The only problem with this approach was the double-vs-int types.
  // If the type is meant to be a double we want to allow its definition as
  // "min:0, max:0" instead of just forcing it to be only "min:0.0, max:0.0".
  // If we have "minimum" or "maximum", and we have a Double schema object,
  // treat this object as a Double (even if both min and max are integers).
  if (has_min_max && base_schema && base_schema->GetType() == ValueType::Double)
    return PropType::GetTypeStringFromType(ValueType::Double);

  // If we have at least one "minimum" or "maximum" that is Double,
  // it's a Double.
  const base::Value* value = nullptr;
  if (dict->Get(commands::attributes::kNumeric_Min, &value) &&
      value->IsType(base::Value::TYPE_DOUBLE))
    return PropType::GetTypeStringFromType(ValueType::Double);
  if (dict->Get(commands::attributes::kNumeric_Max, &value) &&
      value->IsType(base::Value::TYPE_DOUBLE))
    return PropType::GetTypeStringFromType(ValueType::Double);

  // If we have "minimum" or "maximum", it's an Integer.
  if (has_min_max)
    return PropType::GetTypeStringFromType(ValueType::Int);

  // If we have "minLength" or "maxLength", it's a String.
  if (dict->HasKey(commands::attributes::kString_MinLength) ||
      dict->HasKey(commands::attributes::kString_MaxLength))
    return PropType::GetTypeStringFromType(ValueType::String);

  // If we have "properties", it's an object.
  if (dict->HasKey(commands::attributes::kObject_Properties))
    return PropType::GetTypeStringFromType(ValueType::Object);

  // If we have "enum", it's an array. Detect type from array elements.
  const base::ListValue* list = nullptr;
  if (dict->GetListWithoutPathExpansion(
      commands::attributes::kOneOf_Enum, &list))
    return DetectArrayType(list, base_schema);

  return std::string();
}

bool ObjectSchema::PropFromJsonObject(const std::string& prop_name,
                                      const base::Value& value,
                                      const PropType* base_schema,
                                      Properties* properties,
                                      chromeos::ErrorPtr* error) const {
  const base::DictionaryValue* dict = nullptr;
  CHECK(value.GetAsDictionary(&dict)) << "Unable to get dictionary value";
  std::string type_name;
  if (dict->HasKey(commands::attributes::kType)) {
    if (!dict->GetString(commands::attributes::kType, &type_name))
      return ErrorInvalidTypeInfo(prop_name, error);
  } else {
    type_name = DetectObjectType(dict, base_schema);
  }
  if (type_name.empty()) {
    if (!base_schema)
      return ErrorInvalidTypeInfo(prop_name, error);
    type_name = base_schema->GetTypeAsString();
  }
  std::unique_ptr<PropType> prop = CreatePropType(type_name, prop_name, error);
  if (!prop)
    return false;
  if (!prop->FromJson(dict, base_schema, error)) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kInvalidPropDef,
                                 "Error in definition of property '%s'",
                                 prop_name.c_str());
    return false;
  }
  properties->insert(std::make_pair(prop_name, std::move(prop)));
  return true;
}

}  // namespace buffet
