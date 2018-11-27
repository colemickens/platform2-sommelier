/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <base/logging.h>
#include <base/values.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "runtime_probe/daemon.h"
#include "runtime_probe/statement_parser.h"

namespace {
enum ExitStatus {
  kSuccess = EXIT_SUCCESS,  // 0
  kUnknownError = 1,
  kNeedConfigFilePath = 10,
  kFailToParseConfigFile = 11,
};
}  // namespace

using base::DictionaryValue;

int main(int argc, char* argv[]) {
  // Flags are subject to change
  DEFINE_string(config_file_path, "", "File path to probe statement");
  DEFINE_bool(dbus, false, "Run in the mode to respond DBus call");
  DEFINE_bool(debug, false, "Output debug message");
  brillo::FlagHelper::Init(argc, argv, "ChromeOS runtime probe tool");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  // VLOG uses negative log level. So VLOG(1) used for debugging messgae will
  // only display if min log_level < 0
  const int log_level = FLAGS_debug ? -1 : 0;
  logging::SetMinLogLevel(log_level);
  LOG(INFO) << "Starting Runtime Probe";

  if (FLAGS_dbus) {
    // For testing purpose
    LOG(INFO) << "Running in daemon mode";
    runtime_probe::Daemon daemon;
    return daemon.Run();
  }

  LOG(INFO) << "Running in CLI mode";

  if (FLAGS_config_file_path.empty()) {
    LOG(ERROR) << "Please specify config_file path";
    return ExitStatus::kNeedConfigFilePath;
  }
  std::unique_ptr<base::DictionaryValue> statement_dv =
      runtime_probe::ParseProbeConfig(FLAGS_config_file_path);
  if (statement_dv == nullptr) {
    LOG(ERROR) << "Failed to parse probestatement from config file";
    return ExitStatus::kFailToParseConfigFile;
  }
  // TODO(hmchu): probe logic starts here

  return ExitStatus::kSuccess;
}
