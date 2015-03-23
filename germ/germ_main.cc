// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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

int Launch(const std::string& name, const std::string& command_line) {
  scoped_ptr<IBinder> proxy(GetServiceManager()->GetService("germ"));
  scoped_ptr<IGerm> germ(protobinder::BinderToInterface<IGerm>(proxy.get()));
  if (!germ) {
    LOG(FATAL) << "GetService(germ) failed";
  }
  LaunchRequest request;
  LaunchResponse response;
  request.set_name(name);
  request.set_command_line(command_line);
  germ->Launch(&request, &response);
  return response.status();
}

}  // namespace

}  // namespace germ

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  DEFINE_string(name, "",
                "Name of the service, cannot be empty.");
  DEFINE_string(executable, "",
                "Full path of the executable, cannot be empty.");
  DEFINE_bool(interactive, false, "Run in the foreground.");
  DEFINE_bool(shell, false,
              "Don't actually run the service, just launch a shell. "
              "Implies --interactive.");

  chromeos::FlagHelper::Init(argc, argv, "germ");
  chromeos::InitLog(chromeos::kLogToSyslog);

  int ret = 0;
  // FLAGS_shell implies FLAGS_interactive.
  if (FLAGS_interactive || FLAGS_shell) {
    germ::Launcher launcher;
    std::string executable =
        FLAGS_shell ? std::string(kShellExecutablePath) : FLAGS_executable;
    ret = launcher.RunInteractive(FLAGS_name, executable);
  } else {
    ret = germ::Launch(FLAGS_name, FLAGS_executable);
  }
  return ret;
}
