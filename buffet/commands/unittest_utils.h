// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_UNITTEST_UTILS_H_
#define BUFFET_COMMANDS_UNITTEST_UTILS_H_

#include <memory>
#include <string>

#include <base/values.h>

#include "buffet/commands/prop_types.h"
#include "buffet/commands/prop_values.h"

namespace buffet {
namespace unittests {

// Helper method to create base::Value from a string as a smart pointer.
// For ease of definition in C++ code, double-quotes in the source definition
// are replaced with apostrophes.
std::unique_ptr<base::Value> CreateValue(const char* json);

// Helper method to create a JSON dictionary object from a string.
std::unique_ptr<base::DictionaryValue> CreateDictionaryValue(const char* json);

// Converts a JSON value to a string. It also converts double-quotes to
// apostrophes for easy comparisons in C++ source code.
std::string ValueToString(const base::Value* value);

template <typename PV, typename T> std::shared_ptr<const PV>
make_prop_value(const PropType* type, const T& value) {
  auto result = std::make_shared<PV>(type);
  result->SetValue(value);
  return result;
}

inline std::shared_ptr<const IntValue> make_int_prop_value(int value) {
  static const PropType* int_prop_type = new IntPropType();
  return make_prop_value<IntValue, int>(int_prop_type, value);
}

inline std::shared_ptr<const DoubleValue> make_double_prop_value(double value) {
  static const PropType* double_prop_type = new DoublePropType();
  return make_prop_value<DoubleValue, double>(double_prop_type, value);
}

inline std::shared_ptr<const BooleanValue> make_bool_prop_value(bool value) {
  static const PropType* boolean_prop_type = new BooleanPropType();
  return make_prop_value<BooleanValue, bool>(boolean_prop_type, value);
}

inline std::shared_ptr<const StringValue>
make_string_prop_value(const std::string& value) {
  static const PropType* string_prop_type = new StringPropType();
  return make_prop_value<StringValue, std::string>(string_prop_type, value);
}

}  // namespace unittests
}  // namespace buffet

#endif  // BUFFET_COMMANDS_UNITTEST_UTILS_H_
