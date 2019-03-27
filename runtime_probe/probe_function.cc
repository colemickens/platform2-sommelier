// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <base/values.h>
#include <base/files/scoped_temp_dir.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_writer.h>
#include <brillo/process.h>

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

bool ProbeFunction::InvokeHelper(std::string* result) const {
  std::string tmp_json_string;
  base::JSONWriter::Write(*raw_value_, &tmp_json_string);

  // TODO(itspeter): instead of fork, fold the logic to debugd (with proper
  // minijail / seccomp sandbox) and consume the return value of a string.
  std::unique_ptr<brillo::Process> process(new brillo::ProcessImpl());
  process->AddArg("/usr/bin/runtime_probe_helper");
  process->AddArg(tmp_json_string);
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  const std::string output_file =
      temp_dir.GetPath().Append("helper_out").value();
  process->RedirectOutput(output_file);
  int status = process->Run();
  if (status) {
    VLOG(1) << "Helper returns non-zero value (" << status << ") when "
            << "processing statement " << tmp_json_string;
    return false;
  }
  base::ReadFileToString(base::FilePath(output_file), result);

  VLOG(1) << "EvalInHelper returns " << *result;
  return true;
}

int ProbeFunction::EvalInHelper(std::string* output) const {
  return 0;
}

}  // namespace runtime_probe
