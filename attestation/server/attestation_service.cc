// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/server/attestation_service.h"

#include <string>

#include <base/bind.h>
#include <base/time/time.h>

#include "attestation/common/dbus_interface.pb.h"

namespace attestation {

namespace {

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
static void HandleSynchronousDBusMethodCall(
    const DBusMethodCallHandler& handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  ResponsePtr response = handler.Run(method_call);
  if (!response) {
    response = dbus::Response::FromMethodCall(method_call);
  }

  response_sender.Run(response.Pass());
}

}  // namespace

AttestationService::AttestationService()
    : start_time_(base::Time()),
      bus_(nullptr),
      attestation_dbus_object_(nullptr) {}

AttestationService::~AttestationService() {}

void AttestationService::Init() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  CHECK(bus_->Connect());

  attestation_dbus_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(kAttestationServicePath));
  CHECK(attestation_dbus_object_);

  ExportDBusMethod(kStatsMethod,
                   base::Bind(&AttestationService::HandleStatsMethod,
                              base::Unretained(this)));

  CHECK(bus_->RequestOwnershipAndBlock(kAttestationServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kAttestationServiceName;

  start_time_ = base::Time::Now();
}

void AttestationService::ExportDBusMethod(
    const std::string& method_name,
    const DBusMethodCallHandler& handler) {
  CHECK(attestation_dbus_object_->ExportMethodAndBlock(
      kAttestationInterface, method_name,
      base::Bind(&HandleSynchronousDBusMethodCall, handler)));
}

ResponsePtr AttestationService::HandleStatsMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Received call to stats method.";
  StatsResponse stats;
  stats.set_uptime((base::Time::Now() - start_time_).InSeconds());
  ResponsePtr response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(stats);
  return response.Pass();
}

}  // namespace attestation
