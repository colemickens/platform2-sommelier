// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <base/values.h>
#include <base/json/json_writer.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "runtime_probe/probe_function.h"

namespace runtime_probe {

using DataType = typename ProbeFunction::DataType;

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

  // function_name is the only key exists in the dictionary */
  const auto& function_name = it.key();
  const auto& kwargs = it.value();

  if (registered_functions_.find(function_name) ==
      registered_functions_.end()) {
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

  std::unique_ptr<ProbeFunction> ret_value =
      registered_functions_[function_name](*dict_args);
  ret_value->raw_value_ = dict_value.CreateDeepCopy();

  return ret_value;
}

constexpr auto kDebugdRunProbeHelperMethodName = "EvaluateProbeFunction";
constexpr auto kDebugdRunProbeHelperDefaultTimeoutMs = 10 * 1000;  // in ms

bool ProbeFunction::InvokeHelper(std::string* result) const {
  std::string tmp_json_string;
  base::JSONWriter::Write(*raw_value_, &tmp_json_string);

  dbus::Bus::Options ops;
  ops.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(std::move(ops)));

  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system D-Bus service.";
    return false;
  }

  dbus::ObjectProxy* object_proxy = bus->GetObjectProxy(
      debugd::kDebugdServiceName, dbus::ObjectPath(debugd::kDebugdServicePath));

  dbus::MethodCall method_call(debugd::kDebugdInterface,
                               kDebugdRunProbeHelperMethodName);
  dbus::MessageWriter writer(&method_call);

  writer.AppendString(GetFunctionName());
  writer.AppendString(tmp_json_string);

  std::unique_ptr<dbus::Response> response = object_proxy->CallMethodAndBlock(
      &method_call, kDebugdRunProbeHelperDefaultTimeoutMs);
  if (!response) {
    LOG(ERROR) << "Failed to issue D-Bus call to method "
               << kDebugdRunProbeHelperMethodName
               << " of debugd D-Bus interface.";
    return false;
  }

  dbus::MessageReader reader(response.get());
  if (!reader.PopString(result)) {
    LOG(ERROR) << "Failed to read probe_result from debugd.";
    return false;
  }

  int32_t exit_code = -1;
  if (!reader.PopInt32(&exit_code)) {
    LOG(ERROR) << "Failed to read exit_code from debugd.";
    return false;
  }

  if (exit_code) {
    VLOG(1) << "Helper returns non-zero value (" << exit_code << ") when "
            << "processing statement " << result;
    return false;
  }

  VLOG(1) << "EvalInHelper returns " << *result;
  return true;
}

int ProbeFunction::EvalInHelper(std::string* output) const {
  return 0;
}

}  // namespace runtime_probe
