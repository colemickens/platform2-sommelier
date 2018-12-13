/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef RUNTIME_PROBE_PROBE_FUNCTION_ARGUMENT_H_
#define RUNTIME_PROBE_PROBE_FUNCTION_ARGUMENT_H_

#include <memory>
#include <vector>

#include "base/values.h"

namespace runtime_probe {

/*
 * To know how to define an argument parser and use it in your probe function,
 * please check "functions/shell.h" as an example.  It should be well commented.
 *
 * Currently, we only supports the following types of arguments:
 *   - std::string
 *   - int
 *   - bool
 *   - double
 *   - std::vector<std::string>
 *   - std::vector<std::unique_ptr<ProbeFunction>>
 *
 * Arguments can have default value, except for
 * std::vector<std::unique_ptr<ProbeFunction>>.
 */

class ProbeFunction;

template <typename T>
bool ParseArgument(const char* function_name,
                   const char* member_name,
                   T* member,
                   const base::Value& value);

template <typename T>
bool ParseArgument(const char* function_name,
                   const char* member_name,
                   T* member,
                   const base::DictionaryValue& dict_value) {
  const base::Value* value;
  if (!dict_value.Get(member_name, &value)) {
    LOG(ERROR) << function_name << ": `" << member_name << "`not found";
    return false;
  }

  return ParseArgument(function_name, member_name, member, *value);
}

template <typename T>
bool ParseArgument(const char* function_name,
                   const char* member_name,
                   T* member,
                   const base::DictionaryValue& dict_value,
                   const T&& default_value) {
  if (!dict_value.HasKey(member_name)) {
    *member = default_value;
    return true;
  }

  return ParseArgument(function_name, member_name, member, dict_value);
}

template <>
bool ParseArgument<std::vector<std::unique_ptr<ProbeFunction>>>(
    const char* function_name,
    const char* member_name,
    std::vector<std::unique_ptr<ProbeFunction>>* member,
    const base::DictionaryValue& dict_value,
    const std::vector<std::unique_ptr<ProbeFunction>>&& default_value) = delete;

/* We assume that |function_name|, |instance|, |dict_value| are available in the
 * scope this macro is called.  See `functions/shell.h` about how this macro is
 * used.
 */
#define PARSE_ARGUMENT(member_name, ...)                                \
  ParseArgument(function_name, #member_name, &instance->member_name##_, \
                dict_value, ##__VA_ARGS__)

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_PROBE_FUNCTION_ARGUMENT_H_
