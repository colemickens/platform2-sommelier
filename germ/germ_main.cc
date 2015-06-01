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
#include <soma/sandbox_spec_reader.h>
#include <soma/read_only_sandbox_spec.h>

#include "germ/germ_zygote.h"
#include "germ/launcher.h"
#include "germ/proto_bindings/soma_sandbox_spec.pb.h"

namespace {
const char kShellExecutablePath[] = "/bin/sh";
}  // namespace

int main(int argc, char** argv) {
  DEFINE_string(name, "", "Name of service");
  DEFINE_string(spec, "", "Path to SandboxSpec");
  DEFINE_bool(shell, false,
              "Don't actually run the service, just launch a shell");

  chromeos::FlagHelper::Init(argc, argv,
                             "germ [OPTIONS] [-- EXECUTABLE [ARGUMENTS...]]");
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  int status = 0;

  // Should we launch a SandboxSpec?
  if (!FLAGS_spec.empty()) {
    // Read and launch a SandboxSpec.
    // TODO(jorgelo): Allow launching a shell.
    soma::SandboxSpecReader ssr;
    base::FilePath path(FLAGS_spec);
    std::unique_ptr<soma::SandboxSpec> cspec = ssr.Read(path);
    if (!cspec) {
      // SandboxSpecReader::Path() will print an appropriate error.
      return 1;
    }
    LOG(INFO) << "Read SandboxSpec '" << FLAGS_spec << "'";

    germ::GermZygote zygote;
    zygote.Start();
    pid_t pid = -1;
    if (!zygote.StartContainer(*cspec, &pid)) {
      LOG(ERROR) << "Failed to launch '" << cspec->name() << "'";
      return 1;
    }
    LOG(INFO) << "Launched '" << cspec->name() << "' with init PID " << pid;
    status = 0;
  } else {
    // Launch an executable.
    base::AtExitManager exit_manager;
    germ::Launcher launcher;

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
