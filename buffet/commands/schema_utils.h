// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_SCHEMA_UTILS_H_
#define BUFFET_COMMANDS_SCHEMA_UTILS_H_

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <base/values.h>
#include <chromeos/errors/error.h>

namespace buffet {

class PropValue;
class ObjectSchema;

namespace native_types {
// C++ representation of object values.
using Object = std::map<std::string, std::shared_ptr<const PropValue>>;
}  // namespace native_types
// Converts an object to string.
std::string ToString(const native_types::Object& obj);

// InheritableAttribute class is used for specifying various command parameter
// attributes that can be inherited from a base (parent) schema.
// The |value| still specifies the actual attribute values, whether it
// is inherited or overridden, while |is_inherited| can be used to identify
// if the attribute was inherited (true) or overridden (false).
template<typename T>
class InheritableAttribute {
 public:
  InheritableAttribute() = default;
  explicit InheritableAttribute(T val)
      : value(std::move(val)), is_inherited(true) {}
  InheritableAttribute(T val, bool inherited)
      : value(std::move(val)), is_inherited(inherited) {}
  T value{};
  bool is_inherited{true};
};

// A bunch of helper function to create base::Value for specific C++ classes,
// including vectors of types. These are used in template classes below
// to simplify specialization logic.
std::unique_ptr<base::Value> TypedValueToJson(bool value,
                                              chromeos::ErrorPtr* error);
std::unique_ptr<base::Value> TypedValueToJson(int value,
                                              chromeos::ErrorPtr* error);
std::unique_ptr<base::Value> TypedValueToJson(double value,
                                              chromeos::ErrorPtr* error);
std::unique_ptr<base::Value> TypedValueToJson(const std::string& value,
                                              chromeos::ErrorPtr* error);
std::unique_ptr<base::Value> TypedValueToJson(const native_types::Object& value,
                                              chromeos::ErrorPtr* error);
template<typename T>
std::unique_ptr<base::Value> TypedValueToJson(const std::vector<T>& values,
                                              chromeos::ErrorPtr* error) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (const auto& v : values) {
    auto json = TypedValueToJson(v, error);
    if (!json)
      return std::unique_ptr<base::Value>();
    list->Append(json.release());
  }
  return std::move(list);
}

// Similarly to CreateTypedValue() function above, the following overloaded
// helper methods allow to extract specific C++ data types from base::Value.
// Also used in template classes below to simplify specialization logic.
bool TypedValueFromJson(const base::Value* value_in,
                        const ObjectSchema* object_schema,
                        bool* value_out, chromeos::ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const ObjectSchema* object_schema,
                        int* value_out, chromeos::ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const ObjectSchema* object_schema,
                        double* value_out, chromeos::ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const ObjectSchema* object_schema,
                        std::string* value_out, chromeos::ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const ObjectSchema* object_schema,
                        native_types::Object* value_out,
                        chromeos::ErrorPtr* error);

bool operator==(const native_types::Object& obj1,
                const native_types::Object& obj2);

// CompareValue is a helper function to help with implementing EqualsTo operator
// for various data types. For most scalar types it is using operator==(),
// however, for floating point values, rounding errors in binary representation
// of IEEE floats/doubles can cause straight == comparison to fail for seemingly
// equivalent values. For these, use approximate comparison with the error
// margin equal to the epsilon value defined for the corresponding data type.
// This is used when looking up values for implementation of OneOf constraints
// which should work reliably for floating points also ("number" type).

// Compare exact types using ==.
template<typename T>
inline typename std::enable_if<!std::is_floating_point<T>::value, bool>::type
CompareValue(const T& v1, const T& v2) {
  return v1 == v2;
}

// Compare non-exact types (such as double) using precision margin (epsilon).
template<typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, bool>::type
CompareValue(const T& v1, const T& v2) {
  return std::abs(v1 - v2) <= std::numeric_limits<T>::epsilon();
}

}  // namespace buffet

#endif  // BUFFET_COMMANDS_SCHEMA_UTILS_H_
