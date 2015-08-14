// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_SCHEMA_UTILS_H_
#define LIBWEAVE_SRC_COMMANDS_SCHEMA_UTILS_H_

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <base/values.h>
#include <weave/error.h>

namespace weave {

class PropType;
class PropValue;
class ObjectSchema;
class ObjectValue;

// C++ representation of object values.
using ValueMap = std::map<std::string, std::shared_ptr<const PropValue>>;

// C++ representation of array of values.
using ValueVector = std::vector<std::shared_ptr<const PropValue>>;

// Converts an object to string.
std::string ToString(const ValueMap& obj);

// Converts an array to string.
std::string ToString(const ValueVector& arr);

// InheritableAttribute class is used for specifying various command parameter
// attributes that can be inherited from a base (parent) schema.
// The |value| still specifies the actual attribute values, whether it
// is inherited or overridden, while |is_inherited| can be used to identify
// if the attribute was inherited (true) or overridden (false).
template <typename T>
class InheritableAttribute final {
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
std::unique_ptr<base::FundamentalValue> TypedValueToJson(bool value);
std::unique_ptr<base::FundamentalValue> TypedValueToJson(int value);
std::unique_ptr<base::FundamentalValue> TypedValueToJson(double value);
std::unique_ptr<base::StringValue> TypedValueToJson(const std::string& value);
std::unique_ptr<base::DictionaryValue> TypedValueToJson(const ValueMap& value);
std::unique_ptr<base::ListValue> TypedValueToJson(const ValueVector& value);
template <typename T>
std::unique_ptr<base::ListValue> TypedValueToJson(
    const std::vector<T>& values) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (const auto& v : values) {
    auto json = TypedValueToJson(v);
    CHECK(json);
    list->Append(json.release());
  }
  return list;
}

// Similarly to TypedValueToJson() function above, the following overloaded
// helper methods allow to extract specific C++ data types from base::Value.
// Also used in template classes below to simplify specialization logic.
// TODO(vitalybuka): Fix this. Interface is misleading. Seeing PropType internal
// type validation is expected. In reality only ValueMap and ValueVector do
// validation.
bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        bool* value_out,
                        ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        int* value_out,
                        ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        double* value_out,
                        ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        std::string* value_out,
                        ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        ValueMap* value_out,
                        ErrorPtr* error);
bool TypedValueFromJson(const base::Value* value_in,
                        const PropType* type,
                        ValueVector* value_out,
                        ErrorPtr* error);

bool operator==(const ValueMap& obj1, const ValueMap& obj2);
bool operator==(const ValueVector& arr1, const ValueVector& arr2);

// CompareValue is a helper function to help with implementing EqualsTo operator
// for various data types. For most scalar types it is using operator==(),
// however, for floating point values, rounding errors in binary representation
// of IEEE floats/doubles can cause straight == comparison to fail for seemingly
// equivalent values. For these, use approximate comparison with the error
// margin equal to the epsilon value defined for the corresponding data type.
// This is used when looking up values for implementation of OneOf constraints
// which should work reliably for floating points also ("number" type).

// Compare exact types using ==.
template <typename T>
inline typename std::enable_if<!std::is_floating_point<T>::value, bool>::type
CompareValue(const T& v1, const T& v2) {
  return v1 == v2;
}

// Compare non-exact types (such as double) using precision margin (epsilon).
template <typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, bool>::type
CompareValue(const T& v1, const T& v2) {
  return std::abs(v1 - v2) <= std::numeric_limits<T>::epsilon();
}

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_SCHEMA_UTILS_H_
