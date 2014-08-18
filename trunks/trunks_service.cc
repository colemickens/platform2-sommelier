// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/logging.h>
#include <chromeos/libminijail.h>
#include <chromeos/minijail/minijail.h>

#include "trunks/dbus_interface.h"
#include "trunks/error_codes.h"
#include "trunks/tpm_handle.h"
#include "trunks/trunks_service.h"

namespace trunks {

namespace {

const uid_t kTrunksUID = 248;
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
  dbus::MessageReader reader(method_call);
  std::string value;
  reader.PopString(&value);

  // Perform TPM operation here with |value| = command
  // currently TPM_RC is also stored at |value|
  dbus::MessageWriter writer(response.get());
  writer.AppendString(value);
  response_sender.Run(response.Pass());
}

void TrunksService::InitMinijailSandbox() {
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  struct minijail* jail = minijail->New();
  minijail->DropRoot(jail, kTrunksUser, kTrunksGroup);
  minijail_use_seccomp_filter(jail);
  minijail_log_seccomp_filter_failures(jail);
  minijail_no_new_privs(jail);
  minijail_parse_seccomp_filters(jail, kTrunksSeccompPath);
  minijail_enter(jail);
  CHECK_EQ(getuid(), kTrunksUID)
      << "Trunks Daemon was not able to drop to trunks user.";
  minijail->Destroy(jail);
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
