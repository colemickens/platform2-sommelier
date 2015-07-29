// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_UNITTEST_UTILS_H_
#define LIBWEAVE_SRC_COMMANDS_UNITTEST_UTILS_H_

#include <memory>
#include <string>

#include <base/values.h>
#include <gtest/gtest.h>

#include "libweave/src/commands/prop_types.h"
#include "libweave/src/commands/prop_values.h"

namespace weave {
namespace unittests {

// Helper method to create base::Value from a string as a smart pointer.
// For ease of definition in C++ code, double-quotes in the source definition
// are replaced with apostrophes.
std::unique_ptr<base::Value> CreateValue(const std::string& json);

// Helper method to create a JSON dictionary object from a string.
std::unique_ptr<base::DictionaryValue> CreateDictionaryValue(
    const std::string& json);

inline bool IsEqualValue(const base::Value& val1, const base::Value& val2) {
  return val1.Equals(&val2);
}

template <typename PropVal, typename T>
std::unique_ptr<const PropVal> make_prop_value(const T& value) {
  std::unique_ptr<PropVal> result{
      new PropVal{PropType::Create(GetValueType<T>())}};
  if (!result->SetValue(value, nullptr))
    return nullptr;
  return std::move(result);
}

inline std::unique_ptr<const IntValue> make_int_prop_value(int value) {
  return make_prop_value<IntValue, int>(value);
}

inline std::unique_ptr<const DoubleValue> make_double_prop_value(double value) {
  return make_prop_value<DoubleValue, double>(value);
}

inline std::unique_ptr<const BooleanValue> make_bool_prop_value(bool value) {
  return make_prop_value<BooleanValue, bool>(value);
}

inline std::unique_ptr<const StringValue> make_string_prop_value(
    const std::string& value) {
  return make_prop_value<StringValue, std::string>(value);
}

}  // namespace unittests
}  // namespace weave

#define EXPECT_JSON_EQ(expected, actual)                                   \
  EXPECT_PRED2(unittests::IsEqualValue, *unittests::CreateValue(expected), \
               actual)

#endif  // LIBWEAVE_SRC_COMMANDS_UNITTEST_UTILS_H_
