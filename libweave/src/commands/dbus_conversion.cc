// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/dbus_conversion.h"

#include <set>
#include <string>
#include <vector>

#include "libweave/src/commands/object_schema.h"
#include "libweave/src/commands/prop_types.h"
#include "libweave/src/commands/prop_values.h"

namespace weave {

namespace {

// Helpers for JsonToAny().
template <typename T>
chromeos::Any ValueToAny(const base::Value& json,
                         bool (base::Value::*fnc)(T*) const) {
  T val;
  CHECK((json.*fnc)(&val));
  return val;
}

chromeos::Any ValueToAny(const base::Value& json);

template <typename T>
chromeos::Any ListToAny(const base::ListValue& list,
                        bool (base::Value::*fnc)(T*) const) {
  std::vector<T> result;
  result.reserve(list.GetSize());
  for (const base::Value* v : list) {
    T val;
    CHECK((v->*fnc)(&val));
    result.push_back(val);
  }
  return result;
}

chromeos::Any DictListToAny(const base::ListValue& list) {
  std::vector<chromeos::VariantDictionary> result;
  result.reserve(list.GetSize());
  for (const base::Value* v : list) {
    const base::DictionaryValue* dict = nullptr;
    CHECK(v->GetAsDictionary(&dict));
    result.push_back(DictionaryToDBusVariantDictionary(*dict));
  }
  return result;
}

chromeos::Any ListListToAny(const base::ListValue& list) {
  std::vector<chromeos::Any> result;
  result.reserve(list.GetSize());
  for (const base::Value* v : list)
    result.push_back(ValueToAny(*v));
  return result;
}

// Converts a JSON value into an Any so it can be sent over D-Bus using
// UpdateState D-Bus method from Buffet.
chromeos::Any ValueToAny(const base::Value& json) {
  chromeos::Any prop_value;
  switch (json.GetType()) {
    case base::Value::TYPE_BOOLEAN:
      prop_value = ValueToAny<bool>(json, &base::Value::GetAsBoolean);
      break;
    case base::Value::TYPE_INTEGER:
      prop_value = ValueToAny<int>(json, &base::Value::GetAsInteger);
      break;
    case base::Value::TYPE_DOUBLE:
      prop_value = ValueToAny<double>(json, &base::Value::GetAsDouble);
      break;
    case base::Value::TYPE_STRING:
      prop_value = ValueToAny<std::string>(json, &base::Value::GetAsString);
      break;
    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* dict = nullptr;
      CHECK(json.GetAsDictionary(&dict));
      prop_value = DictionaryToDBusVariantDictionary(*dict);
      break;
    }
    case base::Value::TYPE_LIST: {
      const base::ListValue* list = nullptr;
      CHECK(json.GetAsList(&list));
      if (list->empty()) {
        // We don't know type of objects this list intended for, so we just use
        // vector<chromeos::Any>.
        prop_value = ListListToAny(*list);
        break;
      }
      auto type = (*list->begin())->GetType();
      for (const base::Value* v : *list)
        CHECK_EQ(v->GetType(), type);

      switch (type) {
        case base::Value::TYPE_BOOLEAN:
          prop_value = ListToAny<bool>(*list, &base::Value::GetAsBoolean);
          break;
        case base::Value::TYPE_INTEGER:
          prop_value = ListToAny<int>(*list, &base::Value::GetAsInteger);
          break;
        case base::Value::TYPE_DOUBLE:
          prop_value = ListToAny<double>(*list, &base::Value::GetAsDouble);
          break;
        case base::Value::TYPE_STRING:
          prop_value = ListToAny<std::string>(*list, &base::Value::GetAsString);
          break;
        case base::Value::TYPE_DICTIONARY:
          prop_value = DictListToAny(*list);
          break;
        case base::Value::TYPE_LIST:
          // We can't support Any{vector<vector<>>} as the type is only known
          // in runtime when we need to instantiate templates in compile time.
          // We can use Any{vector<Any>} instead.
          prop_value = ListListToAny(*list);
          break;
        default:
          LOG(FATAL) << "Unsupported JSON value type for list element: "
                     << (*list->begin())->GetType();
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected JSON value type: " << json.GetType();
      break;
  }
  return prop_value;
}

bool ErrorMissingProperty(chromeos::ErrorPtr* error,
                          const tracked_objects::Location& location,
                          const char* param_name) {
  chromeos::Error::AddToPrintf(error, location, errors::commands::kDomain,
                               errors::commands::kPropertyMissing,
                               "Required parameter missing: %s", param_name);
  return false;
}

}  // namespace

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

chromeos::VariantDictionary ObjectToDBusVariant(const ValueMap& object) {
  chromeos::VariantDictionary dict;
  for (const auto& pair : object) {
    // Since we are inserting the elements from ValueMap which is
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
    // ValueVector instead. Do the conversion.
    ValueVector arr;
    const PropType* item_type = type->GetArray()->GetItemTypePtr();
    if (item_type->ConvertDBusVariantToArray(value, &arr, error))
      result = type->CreateValue(arr, error);
  } else if (type->GetType() == ValueType::Object) {
    // Special case for object types.
    // We expect the |value| to contain chromeos::VariantDictionary, while
    // PropValue must use ValueMap instead. Do the conversion.
    if (!value.IsTypeCompatible<chromeos::VariantDictionary>()) {
      type->GenerateErrorValueTypeMismatch(error);
      return result;
    }
    CHECK(nullptr != type->GetObject()->GetObjectSchemaPtr())
        << "An object type must have a schema defined for it";
    ValueMap obj;
    if (!ObjectFromDBusVariant(type->GetObject()->GetObjectSchemaPtr(),
                               value.Get<chromeos::VariantDictionary>(), &obj,
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
                           ValueMap* obj,
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
        chromeos::Error::AddToPrintf(
            error, FROM_HERE, errors::commands::kDomain,
            errors::commands::kInvalidPropValue,
            "Invalid value for property '%s'", pair.first.c_str());
        return false;
      }
      obj->emplace_hint(obj->end(), pair.first, std::move(prop_value));
    } else if (def_value) {
      obj->emplace_hint(obj->end(), pair.first, def_value->Clone());
    } else {
      ErrorMissingProperty(error, FROM_HERE, pair.first.c_str());
      return false;
    }
    keys_processed.insert(pair.first);
  }

  // Make sure that we processed all the necessary properties and there weren't
  // any extra (unknown) ones specified, unless the schema allows them.
  if (!object_schema->GetExtraPropertiesAllowed()) {
    for (const auto& pair : dict) {
      if (keys_processed.find(pair.first) == keys_processed.end()) {
        chromeos::Error::AddToPrintf(
            error, FROM_HERE, errors::commands::kDomain,
            errors::commands::kUnknownProperty, "Unrecognized property '%s'",
            pair.first.c_str());
        return false;
      }
    }
  }

  return true;
}

// TODO(vitalybuka): Use in buffet_client.
chromeos::VariantDictionary DictionaryToDBusVariantDictionary(
    const base::DictionaryValue& object) {
  chromeos::VariantDictionary result;

  for (base::DictionaryValue::Iterator it(object); !it.IsAtEnd(); it.Advance())
    result.emplace(it.key(), ValueToAny(it.value()));

  return result;
}

}  // namespace weave
