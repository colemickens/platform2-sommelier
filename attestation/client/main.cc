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
const char kInfoCommand[] = "info";
const char kEndorsementCommand[] = "endorsement";
const char kAttestationKeyCommand[] = "attestation_key";
const char kUsage[] = R"(
Usage: attestation_client <command> [<args>]
Commands:
  create [--user=<email>] [--label=<keylabel>] - Creates a Google-attested key.
      (This is the default command).
  info [--user=<email>] [--label=<keylabel>] - Prints info about a key.
  endorsement - Prints info about the TPM endorsement key.
  attestation_key - Prints info about the TPM attestation key.
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
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const auto& args = command_line->GetArgs();
    if (command_line->HasSwitch("help") || command_line->HasSwitch("h") ||
        args.front() == "help") {
      return EX_USAGE;
    }
    if (args.empty() || args.front() == kCreateCommand) {
      task = base::Bind(&ClientLoop::CallCreateGoogleAttestedKey,
                        weak_factory_.GetWeakPtr(),
                        command_line->GetSwitchValueASCII("label"),
                        command_line->GetSwitchValueASCII("user"));
    } else if (args.front() == kInfoCommand) {
      task = base::Bind(&ClientLoop::CallGetKeyInfo,
                        weak_factory_.GetWeakPtr(),
                        command_line->GetSwitchValueASCII("label"),
                        command_line->GetSwitchValueASCII("user"));
    } else if (args.front() == kEndorsementCommand) {
      task = base::Bind(&ClientLoop::CallGetEndorsementInfo,
                        weak_factory_.GetWeakPtr());
    } else if (args.front() == kAttestationKeyCommand) {
      task = base::Bind(&ClientLoop::CallGetAttestationKeyInfo,
                        weak_factory_.GetWeakPtr());
    } else {
      return EX_USAGE;
    }
    base::MessageLoop::current()->PostTask(FROM_HERE, task);
    return EX_OK;
  }

  template <typename ProtobufType>
  void PrintReplyAndQuit(const ProtobufType& reply) {
    reply.PrintDebugString();
    Quit();
  }

  void CallCreateGoogleAttestedKey(const std::string& label,
                                   const std::string& username) {
    CreateGoogleAttestedKeyRequest request;
    request.set_key_label(label);
    request.set_key_type(KEY_TYPE_RSA);
    request.set_key_usage(KEY_USAGE_SIGN);
    request.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
    request.set_username(username);
    attestation_->CreateGoogleAttestedKey(
        request,
        base::Bind(&ClientLoop::PrintReplyAndQuit<CreateGoogleAttestedKeyReply>,
                   weak_factory_.GetWeakPtr()));
  }

  void CallGetKeyInfo(const std::string& label, const std::string& username) {
    GetKeyInfoRequest request;
    request.set_key_label(label);
    request.set_username(username);
    attestation_->GetKeyInfo(
        request,
        base::Bind(&ClientLoop::PrintReplyAndQuit<GetKeyInfoReply>,
                   weak_factory_.GetWeakPtr()));
  }

  void CallGetEndorsementInfo() {
    GetEndorsementInfoRequest request;
    request.set_key_type(KEY_TYPE_RSA);
    attestation_->GetEndorsementInfo(
        request,
        base::Bind(&ClientLoop::PrintReplyAndQuit<GetEndorsementInfoReply>,
                   weak_factory_.GetWeakPtr()));
  }

  void CallGetAttestationKeyInfo() {
    GetAttestationKeyInfoRequest request;
    request.set_key_type(KEY_TYPE_RSA);
    attestation_->GetAttestationKeyInfo(
        request,
        base::Bind(&ClientLoop::PrintReplyAndQuit<GetAttestationKeyInfoReply>,
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
