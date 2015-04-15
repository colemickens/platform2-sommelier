// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>
#include <soma/container_spec_reader.h>
#include <soma/read_only_container_spec.h>

#include "germ/germ_client.h"

namespace {
const char kShellExecutablePath[] = "/bin/sh";
}  // namespace

int main(int argc, char** argv) {
  DEFINE_string(name, "", "Name of service");
  DEFINE_string(spec, "", "Path to ContainerSpec");
  DEFINE_bool(shell, false,
              "Don't actually run the service, just launch a shell");

  chromeos::FlagHelper::Init(argc, argv,
                             "germ [OPTIONS] [-- EXECUTABLE [ARGUMENTS...]]");
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  base::AtExitManager exit_manager;
  germ::Launcher launcher;
  int status = 0;

  // Should we launch a ContainerSpec?
  if (!FLAGS_spec.empty()) {
    // Read and launch a ContainerSpec.
    // TODO(jorgelo): Allow launching a shell.
    soma::parser::ContainerSpecReader csr;
    base::FilePath path(FLAGS_spec);
    std::unique_ptr<soma::ContainerSpec> cspec = csr.Read(path);
    if (!cspec) {
      // ContainerSpecReader::Path() will print an appropriate error.
      return 1;
    }
    LOG(INFO) << "Read ContainerSpec '" << FLAGS_spec << "'";
    soma::ReadOnlyContainerSpec spec;
    if (!spec.Init(*cspec)) {
      LOG(ERROR) << "Failed to initialize read-only ContainerSpec";
      return 1;
    }
    if (!launcher.RunInteractiveSpec(spec, &status)) {
      LOG(ERROR) << "Failed to launch '" << spec.name() << "'";
      return 1;
    }
  } else {
    // Launch an executable.
    std::vector<std::string> args =
        base::CommandLine::ForCurrentProcess()->GetArgs();
    // It would be great if we could print the "Usage" message here,
    // but chromeos::FlagHelper does not seem to support that.
    // Instead, we LOG(ERROR) and exit. We don't LOG(FATAL) because we
    // don't need a backtrace or core dump.
    if (args.empty()) {
      LOG(ERROR) << "No executable file provided";
      return 1;
    }
    if (FLAGS_name.empty()) {
      LOG(ERROR) << "Empty service name";
      return 1;
    }

    if (FLAGS_shell) {
      args.clear();
      args.push_back(kShellExecutablePath);
    }
    bool success = launcher.RunInteractiveCommand(FLAGS_name, args, &status);
    if (!success) {
      LOG(ERROR) << "Failed to launch '" << FLAGS_name << "'";
      return 1;
    }
  }
  return status;
}
