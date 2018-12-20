/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/values.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <vboot/crossystem.h>

#include "runtime_probe/config_parser.h"
#include "runtime_probe/daemon.h"
#include "runtime_probe/probe_config.h"

namespace {
enum ExitStatus {
  kSuccess = EXIT_SUCCESS,  // 0
  kUnknownError = 1,
  kConfigFileSyntaxError = 11,
  kFailToParseProbeArgFromConfig = 12,
  kNoPermissionForArbitraryProbeConfig = 13,
};
}  // namespace

// TODO(hmchu): Remove the following two functions once we can trigger this by
// probe statement.
// Test if we can get battery info by ectool.
void DryRunGetBatteryInfo() {
  const std::vector<std::string>& ectool_cmd_arg{"/usr/sbin/ectool", "battery"};
  std::string ectool_output;

  VLOG(1) << "Start ectool battery check\n";
  if (!base::GetAppOutput(ectool_cmd_arg, &ectool_output))
    VLOG(1) << "Failed to execute command \"ectool battery\"";
  else
    VLOG(1) << "ectool battery output:\n" << ectool_output;
}

// Test if we can get storage info of /dev/mmcblk0 from mmc.
void DryRunMMCBlockInfo() {
  const std::vector<std::string>& mmc_cmd_arg{"/usr/bin/mmc", "extcsd", "read",
                                              "/dev/mmcblk0"};
  std::string mmc_cmd_output;
  VLOG(1) << "Start mmc check\n";
  if (!base::GetAppOutput(mmc_cmd_arg, &mmc_cmd_output))
    VLOG(1) << "Failed to execute command \"mmc extcsd read /dev/mmcblk0\"";
  else
    VLOG(1) << "mmc extcsd output:\n" << mmc_cmd_output;
}

std::string GetPathOfRootfsProbeConfig() {
  // TODO(itspeter): Find default statement in rootfs.
  return "/etc/a_not_existed_rootfs_config";
}

// Determine if allowed to load probe config assigned from CLI.
// Return 0 on success, non-zero for error.
int GetProbeConfigPath(std::string* probe_config_path,
                       const std::string& probe_config_path_from_cli) {
  // Caller not assigned. Using default one in rootfs.
  if (probe_config_path_from_cli.empty()) {
    VLOG(1) << "No config_file_path specified, picking default config.";
    *probe_config_path = GetPathOfRootfsProbeConfig();
    return 0;
  }

  // Caller assigned, check permission.
  if (VbGetSystemPropertyInt("cros_debug") != 1) {
    LOG(ERROR) << "Arbitrary ProbeConfig is only allowed with cros_debug=1";
    return ExitStatus::kNoPermissionForArbitraryProbeConfig;
  }

  *probe_config_path = probe_config_path_from_cli;
  return 0;
}

int main(int argc, char* argv[]) {
  // Flags are subject to change
  DEFINE_string(config_file_path, "",
                "File path to probe config, empty to use default one");
  DEFINE_bool(dbus, false, "Run in the mode to respond DBus call");
  DEFINE_bool(debug, false, "Output debug message");
  brillo::FlagHelper::Init(argc, argv, "ChromeOS runtime probe tool");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  // VLOG uses negative log level. So VLOG(1) used for debugging messgae will
  // only display if min log_level < 0
  const int log_level = FLAGS_debug ? -1 : 0;
  logging::SetMinLogLevel(log_level);
  LOG(INFO) << "Starting Runtime Probe";

  // For testing purpose on minijail permission.
  if (FLAGS_debug) {
    DryRunGetBatteryInfo();
    DryRunMMCBlockInfo();
  }

  if (FLAGS_dbus) {
    LOG(INFO) << "Running in daemon mode";
    runtime_probe::Daemon daemon;
    return daemon.Run();
  }

  LOG(INFO) << "Running in CLI mode";
  // Invoke as a command line tool. Device can load arbitrary probe config
  // iff cros_debug == 1
  std::string probe_config_path;
  int status_code =
      GetProbeConfigPath(&probe_config_path, FLAGS_config_file_path);
  if (status_code)
    return status_code;

  std::unique_ptr<base::DictionaryValue> config_dv =
      runtime_probe::ParseProbeConfig(probe_config_path);
  if (config_dv == nullptr) {
    LOG(ERROR) << "Failed to parse ProbeConfig from : [" << probe_config_path
               << "]";
    return ExitStatus::kConfigFileSyntaxError;
  }

  auto probe_config =
      runtime_probe::ProbeConfig::FromDictionaryValue(*config_dv);

  if (!probe_config) {
    LOG(ERROR) << "Failed to parse from argument from ProbeConfig\n";
    return ExitStatus::kFailToParseProbeArgFromConfig;
  }
  LOG(INFO) << *(probe_config->Eval());

  return ExitStatus::kSuccess;
}
