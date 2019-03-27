// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/values.h>
#include <brillo/syslog_logging.h>

#include "runtime_probe/probe_function.h"
#include "runtime_probe/utils/config_utils.h"

namespace {
enum ExitStatus {
  kSuccess = EXIT_SUCCESS,  // 0
  kFailedToParseProbeStatementFromArg = 2,
};
}  // namespace

int main(int argc, char* argv[]) {
  brillo::InitLog(brillo::kLogToSyslog);
  if (argc != 2) {
    LOG(ERROR) << "Runtime probe helper consumes a single probe statement";
    return kFailedToParseProbeStatementFromArg;
  }

  std::unique_ptr<base::DictionaryValue> dict_val =
      base::DictionaryValue::From(base::JSONReader::Read(argv[1]));
  if (dict_val == nullptr) {
    LOG(ERROR) << "Failed to parse the probe statement to JSON";
    return kFailedToParseProbeStatementFromArg;
  }

  std::unique_ptr<runtime_probe::ProbeFunction> probe_function =
      runtime_probe::ProbeFunction::FromValue(*dict_val);
  if (probe_function == nullptr) {
    LOG(ERROR) << "Failed to convert a probe statement to probe function";
    return kFailedToParseProbeStatementFromArg;
  }

  std::string output;
  int ret = probe_function->EvalInHelper(&output);
  if (ret)
    return ret;

  printf("%s", output.c_str());
  return ExitStatus::kSuccess;
}
