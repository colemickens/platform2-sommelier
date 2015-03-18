// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/object_schema.h"

#include <algorithm>
#include <limits>

#include <base/logging.h>
#include <base/values.h>
#include <chromeos/map_utils.h>

#include "buffet/commands/prop_types.h"
#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_constants.h"

namespace buffet {

namespace {

// Helper function for to create a PropType based on type string.
// Generates an error if the string identifies an unknown type.
std::unique_ptr<PropType> CreatePropType(const std::string& type_name,
                                         chromeos::ErrorPtr* error) {
  std::unique_ptr<PropType> prop;
  ValueType type;
  if (PropType::GetTypeFromTypeString(type_name, &type))
    prop = PropType::Create(type);
  if (!prop) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kUnknownType,
                                 "Unknown type %s", type_name.c_str());
  }
  return prop;
}

// Generates "no_type_info" error.
void ErrorInvalidTypeInfo(chromeos::ErrorPtr* error) {
  chromeos::Error::AddTo(error, FROM_HERE, errors::commands::kDomain,
                         errors::commands::kNoTypeInfo,
                         "Unable to determine parameter type");
}

// Helper function for PropFromJson to handle the case of parameter being
// defined as a JSON string like this:
//   "prop":"..."
std::unique_ptr<PropType> PropFromJsonString(
    const base::Value& value,
    const PropType* base_schema,
    chromeos::ErrorPtr* error) {
  std::string type_name;
  CHECK(value.GetAsString(&type_name)) << "Unable to get string value";
  std::unique_ptr<PropType> prop = CreatePropType(type_name, error);
  base::DictionaryValue empty;
  if (prop && !prop->FromJson(&empty, base_schema, error))
    prop.reset();

  return prop;
}

// Detects a type based on JSON array. Inspects the first element of the array
// to deduce the PropType from. Returns the string name of the type detected
// or empty string is type detection failed.
std::string DetectArrayType(const base::ListValue* list,
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

// Helper function for PropFromJson to handle the case of parameter being
// defined as a JSON array like this:
//   "prop":[...]
std::unique_ptr<PropType> PropFromJsonArray(
    const base::Value& value,
    const PropType* base_schema,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<PropType> prop;
  const base::ListValue* list = nullptr;
  CHECK(value.GetAsList(&list)) << "Unable to get array value";
  std::string type_name = DetectArrayType(list, base_schema);
  if (type_name.empty()) {
    ErrorInvalidTypeInfo(error);
    return prop;
  }
  base::DictionaryValue array_object;
  array_object.SetWithoutPathExpansion(commands::attributes::kOneOf_Enum,
                                       list->DeepCopy());
  prop = CreatePropType(type_name, error);
  if (prop && !prop->FromJson(&array_object, base_schema, error))
    prop.reset();

  return prop;
}

// Detects a type based on JSON object definition of type. Looks at various
// members such as minimum/maximum constraints, default and enum values to
// try to deduce the underlying type of the element. Returns the string name of
// the type detected or empty string is type detection failed.
std::string DetectObjectType(const base::DictionaryValue* dict,
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

  // If we have "default", try to use it for type detection.
  if (dict->Get(commands::attributes::kDefault, &value)) {
    if (value->IsType(base::Value::TYPE_DOUBLE))
      return PropType::GetTypeStringFromType(ValueType::Double);
    if (value->IsType(base::Value::TYPE_INTEGER))
      return PropType::GetTypeStringFromType(ValueType::Int);
    if (value->IsType(base::Value::TYPE_BOOLEAN))
      return PropType::GetTypeStringFromType(ValueType::Boolean);
    if (value->IsType(base::Value::TYPE_STRING))
      return PropType::GetTypeStringFromType(ValueType::String);
  }

  return std::string();
}

// Helper function for PropFromJson to handle the case of parameter being
// defined as a JSON object like this:
//   "prop":{...}
std::unique_ptr<PropType> PropFromJsonObject(
    const base::Value& value,
    const PropType* base_schema,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<PropType> prop;
  const base::DictionaryValue* dict = nullptr;
  CHECK(value.GetAsDictionary(&dict)) << "Unable to get dictionary value";
  std::string type_name;
  if (dict->HasKey(commands::attributes::kType)) {
    if (!dict->GetString(commands::attributes::kType, &type_name)) {
      ErrorInvalidTypeInfo(error);
      return prop;
    }
  } else {
    type_name = DetectObjectType(dict, base_schema);
  }
  if (type_name.empty()) {
    if (!base_schema) {
      ErrorInvalidTypeInfo(error);
      return prop;
    }
    type_name = base_schema->GetTypeAsString();
  }
  prop = CreatePropType(type_name, error);
  if (prop && !prop->FromJson(dict, base_schema, error))
    prop.reset();

  return prop;
}

}  // anonymous namespace

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
    auto prop_type = PropFromJson(iter.value(), base_schema, error);
    if (prop_type) {
      properties.insert(std::make_pair(iter.key(), std::move(prop_type)));
    } else {
      chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                   errors::commands::kInvalidPropDef,
                                   "Error in definition of property '%s'",
                                   iter.key().c_str());
      return false;
    }
    iter.Advance();
  }
  properties_ = std::move(properties);
  return true;
}

std::unique_ptr<PropType> ObjectSchema::PropFromJson(
    const base::Value& value,
    const PropType* base_schema,
    chromeos::ErrorPtr* error) {
  if (value.IsType(base::Value::TYPE_STRING)) {
    // A string value is a short-hand object specification and provides
    // the parameter type.
    return PropFromJsonString(value, base_schema, error);
  } else if (value.IsType(base::Value::TYPE_LIST)) {
    // One of the enumerated types.
    return PropFromJsonArray(value, base_schema, error);
  } else if (value.IsType(base::Value::TYPE_DICTIONARY)) {
    // Full parameter definition.
    return PropFromJsonObject(value, base_schema, error);
  }
  static const std::map<base::Value::Type, const char*> type_names = {
    {base::Value::TYPE_NULL, "Null"},
    {base::Value::TYPE_BOOLEAN, "Boolean"},
    {base::Value::TYPE_INTEGER, "Integer"},
    {base::Value::TYPE_DOUBLE, "Double"},
    {base::Value::TYPE_STRING, "String"},
    {base::Value::TYPE_BINARY, "Binary"},
    {base::Value::TYPE_DICTIONARY, "Object"},
    {base::Value::TYPE_LIST, "Array"},
  };
  const char* type_name = chromeos::GetOrDefault(type_names, value.GetType(),
                                                 "<unknown>");
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kUnknownType,
                               "Unexpected JSON value type: %s", type_name);
  return {};
}

}  // namespace buffet
