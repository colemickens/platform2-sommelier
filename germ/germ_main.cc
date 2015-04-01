// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <memory>
#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>
#include <protobinder/binder_proxy.h>
#include <psyche/psyche_connection.h>
#include <psyche/psyche_daemon.h>

#include "germ/launcher.h"
#include "germ/proto_bindings/germ.pb.h"
#include "germ/proto_bindings/germ.pb.rpc.h"

namespace {
const char kShellExecutablePath[] = "/bin/sh";
}

namespace germ {

namespace {

class LaunchClient : public psyche::PsycheDaemon {
 public:
  LaunchClient(const std::string& name,
               const std::vector<std::string>& command_line)
      : name_(name), command_line_(command_line), weak_ptr_factory_(this) {}
  ~LaunchClient() override = default;

 private:
  void RequestService() {
    LOG(INFO) << "Requesting service germ";
    psyche_connection()->GetService("germ",
                                    base::Bind(&LaunchClient::ReceiveService,
                                               weak_ptr_factory_.GetWeakPtr()));
  }

  void ReceiveService(scoped_ptr<BinderProxy> proxy) {
    LOG(INFO) << "Received service with handle " << proxy->handle();
    proxy_.reset(proxy.release());
    germ_.reset(protobinder::BinderToInterface<IGerm>(proxy_.get()));
    Launch();
  }

  void Launch() {
    DCHECK(germ_);
    LaunchRequest request;
    LaunchResponse response;
    request.set_name(name_);
    soma::ContainerSpec* spec = request.mutable_spec();
    for (const auto& cmdline_token : command_line_) {
      spec->add_command_line(cmdline_token);
    }
    int binder_ret = germ_->Launch(&request, &response);
    if (binder_ret != 0) {
      LOG(ERROR) << "Failed to launch service '" << name_ << "'";
      // Trigger shut-down of the message loop.
      Quit();
      return;
    }
    LOG(INFO) << "Launched service '" << name_ << "' with pid "
              << response.status();
    // Trigger shut-down of the message loop.
    Quit();
  }

  // PsycheDaemon:
  int OnInit() override {
    int return_code = PsycheDaemon::OnInit();
    if (return_code != EX_OK)
      return return_code;

    base::MessageLoopForIO::current()->PostTask(
        FROM_HERE, base::Bind(&LaunchClient::RequestService,
                              weak_ptr_factory_.GetWeakPtr()));
    return EX_OK;
  }

  std::unique_ptr<BinderProxy> proxy_;
  std::unique_ptr<IGerm> germ_;

  std::string name_;
  std::vector<std::string> command_line_;

  // Keep this member last.
  base::WeakPtrFactory<LaunchClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LaunchClient);
};

}  // namespace

}  // namespace germ

int main(int argc, char** argv) {
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
    base::AtExitManager exit_manager;
    germ::Launcher launcher;
    if (FLAGS_shell) {
      args.clear();
      args.push_back(kShellExecutablePath);
    }
    ret = launcher.RunInteractive(FLAGS_name, args);
  } else {
    germ::LaunchClient client(FLAGS_name, args);
    ret = client.Run();
  }
  return ret;
}
