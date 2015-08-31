// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <sysexits.h>

#include <memory>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/daemons/daemon.h>
#include <chromeos/syslog_logging.h>

#include "tpm_manager/client/dbus_proxy.h"
#include "tpm_manager/common/dbus_interface.pb.h"
#include "tpm_manager/common/print_dbus_interface_proto.h"

namespace tpm_manager {

const char kGetTpmStatusCommand[] = "status";
const char kTakeOwnershipCommand[] = "take_ownership";
const char kUsage[] = R"(
Usage: tpm_manager_client <command> [<args>]
Commands:
  status
      Prints the current status of the Tpm.
  take_ownership
      Takes ownership of the Tpm with a random password.
)";

using ClientLoopBase = chromeos::Daemon;
class ClientLoop : public ClientLoopBase {
 public:
  ClientLoop() = default;
  ~ClientLoop() override = default;

 protected:
  int OnInit() override {
    int exit_code = ClientLoopBase::OnInit();
    if (exit_code != EX_OK) {
      LOG(ERROR) << "Error initializing tpm_manager_client.";
      return exit_code;
    }
    tpm_manager_.reset(new tpm_manager::DBusProxy());
    if (!tpm_manager_->Initialize()) {
      LOG(ERROR) << "Error initializing dbus proxy to tpm_managerd.";
      return EX_UNAVAILABLE;
    }
    exit_code = ScheduleCommand();
    if (exit_code == EX_USAGE) {
      printf("%s", kUsage);
    }
    return exit_code;
  }

  void OnShutdown(int* exit_code) override {
    tpm_manager_.reset();
    ClientLoopBase::OnShutdown(exit_code);
  }

 private:
  // Posts tasks on to the message loop based on command line flags.
  int ScheduleCommand() {
    base::Closure task;
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch("help") || command_line->HasSwitch("h")) {
      return EX_USAGE;
    } else if (command_line->HasSwitch(kGetTpmStatusCommand)) {
      task = base::Bind(&ClientLoop::HandleGetTpmStatus,
                        weak_factory_.GetWeakPtr());
    } else if (command_line->HasSwitch(kTakeOwnershipCommand)) {
      task = base::Bind(&ClientLoop::HandleTakeOwnership,
                        weak_factory_.GetWeakPtr());
    } else {
      // Command line arguments did not match any valid commands.
      LOG(ERROR) << "No Valid Command selected.";
      return EX_USAGE;
    }
    base::MessageLoop::current()->PostTask(FROM_HERE, task);
    return EX_OK;
  }

  void PrintGetTpmStatusReply(const GetTpmStatusReply& reply) {
    if (reply.has_status() && reply.status() == STATUS_NOT_AVAILABLE) {
      LOG(INFO) << "tpm_managerd is not available.";
    } else {
      LOG(INFO) << "TpmStatusReply: " << GetProtoDebugString(reply);
    }
    Quit();
  }

  void HandleGetTpmStatus() {
    GetTpmStatusRequest request;
    tpm_manager_->GetTpmStatus(
        request,
        base::Bind(&ClientLoop::PrintGetTpmStatusReply,
                   weak_factory_.GetWeakPtr()));
  }

  void PrintTakeOwnershipReply(const TakeOwnershipReply& reply) {
    if (reply.has_status() && reply.status() == STATUS_NOT_AVAILABLE) {
      LOG(INFO) << "tpm_managerd is not available.";
    } else {
      LOG(INFO) << "TakeOwnershipReply: " << GetProtoDebugString(reply);
    }
    Quit();
  }

  void HandleTakeOwnership() {
    TakeOwnershipRequest request;
    tpm_manager_->TakeOwnership(request,
                                base::Bind(&ClientLoop::PrintTakeOwnershipReply,
                                           weak_factory_.GetWeakPtr()));
  }

  // Pointer to a DBus proxy to tpm_managerd.
  std::unique_ptr<tpm_manager::TpmManagerInterface> tpm_manager_;

  // Declared last so that weak pointers will be destroyed first.
  base::WeakPtrFactory<ClientLoop> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientLoop);
};

}  // namespace tpm_manager

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  tpm_manager::ClientLoop loop;
  return loop.Run();
}
