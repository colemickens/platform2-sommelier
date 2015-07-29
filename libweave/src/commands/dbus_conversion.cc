// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/dbus_conversion.h"

#include <set>
#include <string>
#include <vector>

#include <chromeos/type_name_undecorate.h>

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
        CHECK_EQ(v->GetType(), type) << "Unsupported different type elements";

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

template <typename T>
std::unique_ptr<base::Value> CreateValue(const T& value,
                                         chromeos::ErrorPtr* error) {
  return std::unique_ptr<base::Value>{new base::FundamentalValue{value}};
}

template <>
std::unique_ptr<base::Value> CreateValue<std::string>(
    const std::string& value,
    chromeos::ErrorPtr* error) {
  return std::unique_ptr<base::Value>{new base::StringValue{value}};
}

template <>
std::unique_ptr<base::Value> CreateValue<chromeos::VariantDictionary>(
    const chromeos::VariantDictionary& value,
    chromeos::ErrorPtr* error) {
  return DictionaryFromDBusVariantDictionary(value, error);
}

template <typename T>
std::unique_ptr<base::ListValue> CreateListValue(const std::vector<T>& value,
                                                 chromeos::ErrorPtr* error) {
  std::unique_ptr<base::ListValue> list{new base::ListValue};

  for (const T& i : value) {
    auto item = CreateValue(i, error);
    if (!item)
      return nullptr;
    list->Append(item.release());
  }

  return list;
}

// Returns false only in case of error. True can be returned if type is not
// matched.
template <typename T>
bool TryCreateValue(const chromeos::Any& any,
                    std::unique_ptr<base::Value>* value,
                    chromeos::ErrorPtr* error) {
  if (any.IsTypeCompatible<T>()) {
    *value = CreateValue(any.Get<T>(), error);
    return *value != nullptr;
  }

  if (any.IsTypeCompatible<std::vector<T>>()) {
    *value = CreateListValue(any.Get<std::vector<T>>(), error);
    return *value != nullptr;
  }

  return true;  // Not an error, we will try different type.
}

template <>
std::unique_ptr<base::Value> CreateValue<chromeos::Any>(
    const chromeos::Any& any,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<base::Value> result;
  if (!TryCreateValue<bool>(any, &result, error) || result)
    return result;

  if (!TryCreateValue<int>(any, &result, error) || result)
    return result;

  if (!TryCreateValue<double>(any, &result, error) || result)
    return result;

  if (!TryCreateValue<std::string>(any, &result, error) || result)
    return result;

  if (!TryCreateValue<chromeos::VariantDictionary>(any, &result, error) ||
      result) {
    return result;
  }

  // This will collapse Any{Any{T}} and vector{Any{T}}.
  if (!TryCreateValue<chromeos::Any>(any, &result, error) || result)
    return result;

  chromeos::Error::AddToPrintf(
      error, FROM_HERE, errors::commands::kDomain,
      errors::commands::kUnknownType, "Type '%s' is not supported.",
      chromeos::UndecorateTypeName(any.GetType().name()).c_str());

  return nullptr;
}

}  // namespace

// TODO(vitalybuka): Use in buffet_client.
chromeos::VariantDictionary DictionaryToDBusVariantDictionary(
    const base::DictionaryValue& object) {
  chromeos::VariantDictionary result;

  for (base::DictionaryValue::Iterator it(object); !it.IsAtEnd(); it.Advance())
    result.emplace(it.key(), ValueToAny(it.value()));

  return result;
}

std::unique_ptr<base::DictionaryValue> DictionaryFromDBusVariantDictionary(
    const chromeos::VariantDictionary& object,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<base::DictionaryValue> result{new base::DictionaryValue};

  for (const auto& pair : object) {
    auto value = CreateValue(pair.second, error);
    if (!value)
      return nullptr;
    result->SetWithoutPathExpansion(pair.first, value.release());
  }

  return result;
}

}  // namespace weave
