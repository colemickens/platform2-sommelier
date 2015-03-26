// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iservice_manager.h>

#include "germ/launcher.h"
#include "germ/proto_bindings/germ.pb.h"
#include "germ/proto_bindings/germ.pb.rpc.h"

namespace {
const char kShellExecutablePath[] = "/bin/sh";
}

namespace germ {

namespace {

int Launch(const std::string& name,
           const std::vector<std::string>& command_line) {
  scoped_ptr<IBinder> proxy(GetServiceManager()->GetService("germ"));
  scoped_ptr<IGerm> germ(protobinder::BinderToInterface<IGerm>(proxy.get()));
  if (!germ) {
    LOG(FATAL) << "GetService(germ) failed";
  }
  LaunchRequest request;
  LaunchResponse response;
  request.set_name(name);
  soma::ContainerSpec* spec = request.mutable_spec();
  for (const auto& cmdline_token : command_line) {
    spec->add_command_line(cmdline_token);
  }
  int ret = germ->Launch(&request, &response);
  LOG(INFO) << "Launched service " << name << " with pid " << response.status();
  return ret;
}

}  // namespace

}  // namespace germ

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  DEFINE_string(name, "", "Name of the service, cannot be empty");
  DEFINE_bool(interactive, false, "Run in the foreground");
  DEFINE_bool(shell, false,
              "Don't actually run the service, just launch a shell");

  chromeos::FlagHelper::Init(argc, argv,
                             "germ [OPTIONS] -- <EXECUTABLE> [ARGUMENTS...]");
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  std::vector<std::string> args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  // It would be great if we could print the "Usage" message here,
  // but chromeos::FlagHelper does not seem to support that.
  // Instead, we LOG(ERROR) and exit. We don't LOG(FATAL) because we don't need
  // a backtrace or core dump.
  if (args.empty()) {
    LOG(ERROR) << "No executable file provided";
    return 1;
  }
  if (FLAGS_name.empty()) {
    LOG(ERROR) << "Empty service name";
    return 1;
  }

  int ret = 0;
  // FLAGS_shell implies FLAGS_interactive.
  if (FLAGS_interactive || FLAGS_shell) {
    germ::Launcher launcher;
    if (FLAGS_shell) {
      args.clear();
      args.push_back(kShellExecutablePath);
    }
    ret = launcher.RunInteractive(FLAGS_name, args);
  } else {
    ret = germ::Launch(FLAGS_name, args);
  }
  return ret;
}
