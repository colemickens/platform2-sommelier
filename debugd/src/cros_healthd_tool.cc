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
// ectool i2cread [NUM_BITS] [PORT] [BATTERY_I2C_ADDRESS (addr8)] [OFFSET]
// Note that [NUM_BITS] can either be 8 or 16.
constexpr char kEctoolCommand[] = "/usr/sbin/ectool";
constexpr char kI2cReadKey[] = "i2cread";
// The specification for smart battery can be found at:
// http://sbs-forum.org/specs/sbdat110.pdf. This states
// that both the temperature and manufacture_date commands
// use the "Read Word" SMBus Protocol, which is 16 bits.
constexpr char kNumBits[] = "16";
// The i2c address is well defined at:
// src/platform/ec/include/battery_smart.h
constexpr char kBatteryI2cAddress[] = "0x16";
// The only ectool argument different across models is the port.
const std::map<std::string, std::string> kModelToPort = {
  {"sona", "2"},
  {"careena", "0"},
  {"dratini", "5"}};
const std::map<std::string, std::string> kMetricNameToOffset = {
  {"temperature_smart", "0x08"},
  {"manufacture_date_smart", "0x1b"}};

}  // namespace

// Note that this is a short-term solution to retrieving battery metrics.
// A long term solution is being discussed at: crbug.com/1047277.
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

  std::string ectool_command;
  auto it = kModelToPort.find(model_name);
  if (it != kModelToPort.end()) {
    const std::string port_number = it->second;
    auto metric_name_it = kMetricNameToOffset.find(metric_name);
    if (metric_name_it != kMetricNameToOffset.end()) {
      const std::string offset = metric_name_it->second;
      ectool_command = base::JoinString({kEctoolCommand, kI2cReadKey, kNumBits,
        port_number, kBatteryI2cAddress, offset}, " ");
    } else {
      DEBUGD_ADD_ERROR(
          error, kErrorPath,
          base::StringPrintf(
              "Failed to find offset for model: %s and metric: %s",
              model_name.c_str(), metric_name.c_str()));
      return false;
    }
  } else {
    DEBUGD_ADD_ERROR(
        error, kErrorPath,
        base::StringPrintf(
            "Failed to find port for model: %s and metric: %s",
            model_name.c_str(), metric_name.c_str()));
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
