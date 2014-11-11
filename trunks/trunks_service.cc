// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_service.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <chromeos/libminijail.h>
#include <chromeos/minijail/minijail.h>

#include "trunks/dbus_interface.h"
#include "trunks/error_codes.h"
#include "trunks/tpm_communication.pb.h"
#include "trunks/tpm_handle.h"

namespace trunks {

namespace {

const uid_t kTrunksUID = 251;
const uid_t kRootUID = 0;
const char kTrunksUser[] = "trunks";
const char kTrunksGroup[] = "trunks";
const char kTrunksSeccompPath[] = "/usr/share/policy/trunksd-seccomp.policy";

}  // namespace

TrunksService::TrunksService()
    : bus_(nullptr),
      trunks_dbus_object_(nullptr) {}

TrunksService::~TrunksService() {}

void TrunksService::Init(TpmHandle* tpm) {
  CHECK_EQ(getuid(), kRootUID) << "Trunks Daemon not initialized as root.";
  tpm_ = tpm;
  CHECK_EQ(tpm->Init(), TPM_RC_SUCCESS)
      << "Trunks Daemon couldn't open the tpm handle.";
  InitMinijailSandbox();
  InitDbusService();
}

void TrunksService::HandleSendCommand(dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  scoped_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  // Get the Command by reading the array passed with the method call.
  dbus::MessageReader reader(method_call);
  TpmCommand tpm_command_proto;
  TpmResponse tpm_response_proto;
  if (!reader.PopArrayOfBytesAsProto(&tpm_command_proto) ||
      !tpm_command_proto.has_command() ||
      tpm_command_proto.command().empty()) {
    LOG(ERROR) << "Trunks Daemon could not read command to send to tpm.";
    tpm_response_proto.set_result_code(TCTI_RC_GENERAL_FAILURE);
    tpm_response_proto.set_response(std::string());
    dbus::MessageWriter writer(response.get());
    writer.AppendProtoAsArrayOfBytes(tpm_response_proto);
    response_sender.Run(response.Pass());
    return;
  }
  // Get the TPM to process the command.
  std::string tpm_response;
  TPM_RC rc = tpm_->SendCommand(tpm_command_proto.command(), &tpm_response);
  tpm_response_proto.set_result_code(rc);
  tpm_response_proto.set_response(tpm_response);
  // Send the response back via dbus.
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(tpm_response_proto);
  response_sender.Run(response.Pass());
  return;
}

void TrunksService::InitMinijailSandbox() {
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  struct minijail* jail = minijail->New();
  minijail->DropRoot(jail, kTrunksUser, kTrunksGroup);
  minijail->UseSeccompFilter(jail, kTrunksSeccompPath);
  minijail->Enter(jail);
  minijail->Destroy(jail);
  CHECK_EQ(getuid(), kTrunksUID)
      << "Trunks Daemon was not able to drop to trunks user.";
}


void TrunksService::InitDbusService() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  CHECK(bus_->Connect());

  trunks_dbus_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(kTrunksServicePath));
  CHECK(trunks_dbus_object_);

  trunks_dbus_object_->ExportMethodAndBlock(kTrunksInterface, kSendCommand,
      base::Bind(&TrunksService::HandleSendCommand, base::Unretained(this)));

  CHECK(bus_->RequestOwnershipAndBlock(kTrunksServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kTrunksServiceName;
}

}  // namespace trunks
