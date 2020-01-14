// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/cros_healthd_tool.h"

#include <map>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/process/launch.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>

#include "debugd/src/error_utils.h"
#include "debugd/src/process_with_output.h"

namespace debugd {

namespace {
constexpr char kErrorPath[] = "org.chromium.debugd.CrosHealthdToolError";
constexpr char kSandboxDirPath[] = "/usr/share/policy/";
constexpr char kBinary[] = "/usr/libexec/diagnostics/cros_healthd_helper";
constexpr char kRunAs[] = "healthd_ec";
constexpr char kCrosHealthdSeccompPolicy[] = "ectool_i2cread-seccomp.policy";
// The ectool command below follows the format:
// ectool i2cread <num_bits> <port> <addr8> <offset>
// Note that <num_bits> can either be 8 or 16.

typedef std::map<std::string, std::vector<std::string>> MetricToEctoolArgs;
typedef std::map<std::string, MetricToEctoolArgs> ModelNameToMetric;
const ModelNameToMetric kModelToEctoolArgs = {
    {"sona",
     {{"temperature_smart",
       {"/usr/sbin/ectool", "i2cread", "16", "2", "0x16", "0x08"}},
      {"manufacture_date_smart",
       {"/usr/sbin/ectool", "i2cread", "16", "2", "0x16", "0x1b"}}}},
    {"careena",
     {{"temperature_smart",
       {"/usr/sbin/ectool", "i2cread", "16", "0", "0x16", "0x08"}},
      {"manufacture_date_smart",
       {"/usr/sbin/ectool", "i2cread", "16", "0", "0x16", "0x1b"}}}}};

}  // namespace

bool CrosHealthdTool::CollectSmartBatteryMetric(brillo::ErrorPtr* error,
                                                const std::string& metric_name,
                                                std::string* output) {
  const auto ectool_seccomp_path =
      base::FilePath{kSandboxDirPath}.Append(base::StringPrintf(
          "%s", kCrosHealthdSeccompPolicy));  // ectool seccomp policy

  if (!base::PathExists(ectool_seccomp_path)) {
    DEBUGD_ADD_ERROR(error, kErrorPath,
                     "Sandbox info is missing for this architecture");
    return false;
  }

  std::string model_name;
  if (!base::GetAppOutputAndError({"cros_config", "/", "name"}, &model_name)) {
    DEBUGD_ADD_ERROR(error, kErrorPath,
                     base::StringPrintf("Failed to run cros_config: %s",
                                        model_name.c_str()));
    return false;
  }
  if (model_name != "sona" && model_name != "careena") {
    DEBUGD_ADD_ERROR(
        error, kErrorPath,
        base::StringPrintf("Failed to access smart battery properties for %s",
                           model_name.c_str()));
    return false;
  }

  std::string ectool_command;
  auto it = kModelToEctoolArgs.find(model_name);
  if (it != kModelToEctoolArgs.end()) {
    const MetricToEctoolArgs& metric_to_ectool_args = it->second;
    auto inner_it = metric_to_ectool_args.find(metric_name);
    if (inner_it != metric_to_ectool_args.end()) {
      const std::vector<std::string>& ectool_args = inner_it->second;
      ectool_command = base::JoinString(ectool_args, " ");
    } else {
      DEBUGD_ADD_ERROR(
          error, kErrorPath,
          base::StringPrintf(
              "Failed to find ectool command for model: %s and metric: %s",
              model_name.c_str(), metric_name.c_str()));
      return false;
    }
  } else {
    DEBUGD_ADD_ERROR(
        error, kErrorPath,
        base::StringPrintf(
            "Failed to find information about metric: %s for model: %s",
            metric_name.c_str(), model_name.c_str()));
    return false;
  }

  // Minijail setup for cros_healthd_helper.
  std::vector<std::string> parsed_args;
  parsed_args.push_back("-c");
  parsed_args.push_back("cap_sys_rawio=e");
  parsed_args.push_back("-b");
  parsed_args.push_back("/dev/cros_ec");

  ProcessWithOutput process;
  process.SandboxAs(kRunAs, kRunAs);
  process.SetSeccompFilterPolicyFile(ectool_seccomp_path.MaybeAsASCII());
  process.InheritUsergroups();
  if (!process.Init(parsed_args)) {
    DEBUGD_ADD_ERROR(error, kErrorPath, "Process initialization failure.");
    return false;
  }

  process.AddArg(kBinary);
  process.AddArg(ectool_command);
  process.Run();
  process.GetOutput(output);
  return true;
}

}  // namespace debugd
