// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ml/daemon.h"

#include <sysexits.h>

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/memory/ref_counted.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>

namespace ml {

Daemon::Daemon() : weak_ptr_factory_(this) {}

Daemon::~Daemon() {}

int Daemon::OnInit() {
  int exit_code = DBusDaemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  InitDBus();
  return 0;
}

void Daemon::InitDBus() {
  LOG(INFO) << "Registering as handler for ML service in D-Bus ...";

  // Get or create the ExportedObject for the ML service.
  dbus::ExportedObject* const ml_service_exported_object =
      bus_->GetExportedObject(dbus::ObjectPath(kMlServicePath));
  CHECK(ml_service_exported_object);

  // Register a handler of the BootstrapMojoConnection method.
  CHECK(ml_service_exported_object->ExportMethodAndBlock(
      kMlInterfaceName, kBootstrapMojoConnectionMethod,
      base::Bind(&Daemon::BootstrapMojoConnection,
                 weak_ptr_factory_.GetWeakPtr())));

  // Take ownership of the ML service.
  CHECK(bus_->RequestOwnershipAndBlock(kMlServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY));

  LOG(INFO) << "D-Bus registration succeeded.";
}

void Daemon::BootstrapMojoConnection(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  LOG(INFO) << "Received bootstrap request";

  // TODO(amoylan): Extract file descriptor from method_call and bind a
  //                ModelProviderImpl to it.

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  response_sender.Run(std::move(response));
}

}  // namespace ml
