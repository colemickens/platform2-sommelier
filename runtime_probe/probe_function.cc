/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <vector>

#include <base/values.h>

#include "runtime_probe/probe_function.h"

namespace runtime_probe {

typedef typename ProbeFunction::DataType DataType;

std::unique_ptr<ProbeFunction> ProbeFunction::FromDictionaryValue(
    const base::DictionaryValue& dict_value) {
  if (dict_value.size() == 0) {
    LOG(ERROR) << "No function name in ProbeFunction dict";
    return nullptr;
  }

  if (dict_value.size() > 1) {
    LOG(ERROR) << "More than 1 function names specified in a "
               << "ProbeFunction dictionary: " << dict_value;
    return nullptr;
  }

  auto it = base::DictionaryValue::Iterator{dict_value};

  /* function_name is the only key exists in the dictionary */
  const auto& function_name = it.key();
  const auto& kwargs = it.value();

  if (REGISTERED_FUNCTIONS.find(function_name) == REGISTERED_FUNCTIONS.end()) {
    // TODO(stimim): Should report an error.
    LOG(ERROR) << "function `" << function_name << "` not found";
    return nullptr;
  }

  if (!kwargs.is_dict()) {
    // TODO(stimim): implement syntax sugar.
    LOG(ERROR) << "function argument must be an dictionary";
    return nullptr;
  }

  const base::DictionaryValue* dict_args;
  kwargs.GetAsDictionary(&dict_args);
  return REGISTERED_FUNCTIONS[function_name](*dict_args);
}
}  // namespace runtime_probe
