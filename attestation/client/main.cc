// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <sysexits.h>

#include <memory>
#include <string>

#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/daemons/daemon.h>
#include <chromeos/syslog_logging.h>

#include "attestation/client/dbus_proxy.h"
#include "attestation/common/attestation_ca.pb.h"
#include "attestation/common/interface.pb.h"

namespace attestation {

const char kCreateCommand[] = "create";
const char kUsage[] = R"(
Usage: attestation_client <command> [<args>]
Commands:
  create - Creates a Google-attested key (this is the default command).
)";

// The Daemon class works well as a client loop as well.
using ClientLoopBase = chromeos::Daemon;

class ClientLoop : public ClientLoopBase {
 public:
  ClientLoop() = default;
  ~ClientLoop() override = default;

 protected:
  int OnInit() override {
    int exit_code = ClientLoopBase::OnInit();
    if (exit_code != EX_OK) {
      return exit_code;
    }
    attestation_.reset(new attestation::DBusProxy());
    if (!attestation_->Initialize()) {
      return EX_UNAVAILABLE;
    }
    exit_code = ScheduleCommand();
    if (exit_code == EX_USAGE) {
      printf("%s", kUsage);
    }
    return exit_code;
  }

  void OnShutdown(int* exit_code) override {
    attestation_.reset();
    ClientLoopBase::OnShutdown(exit_code);
  }

 private:
  // Posts tasks according to the command line options.
  int ScheduleCommand() {
    base::Closure task;
    auto args = base::CommandLine::ForCurrentProcess()->GetArgs();
    if (args.empty() || args.front() == kCreateCommand) {
      task = base::Bind(&ClientLoop::CallCreateGoogleAttestedKey,
                        weak_factory_.GetWeakPtr());
    } else {
      return EX_USAGE;
    }
    base::MessageLoop::current()->PostTask(FROM_HERE, task);
    return EX_OK;
  }

  void HandleCreateGoogleAttestedKeyReply(
      attestation::AttestationStatus status,
      const std::string& certificate,
      const std::string& server_error_details) {
    if (status == attestation::SUCCESS) {
      printf("Success!\n");
    } else {
      printf("Error occurred: %d.\n", status);
      if (!server_error_details.empty()) {
        printf("Server error details: %s\n", server_error_details.c_str());
      }
    }
    Quit();
  }

  void CallCreateGoogleAttestedKey() {
    attestation_->CreateGoogleAttestedKey(
        "test_key",
        attestation::KEY_TYPE_RSA,
        attestation::KEY_USAGE_SIGN,
        attestation::ENTERPRISE_MACHINE_CERTIFICATE,
        base::Bind(&ClientLoop::HandleCreateGoogleAttestedKeyReply,
                   weak_factory_.GetWeakPtr()));
  }

  std::unique_ptr<attestation::AttestationInterface> attestation_;

  // Declare this last so weak pointers will be destroyed first.
  base::WeakPtrFactory<ClientLoop> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientLoop);
};

}  // namespace attestation

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  attestation::ClientLoop loop;
  return loop.Run();
}
