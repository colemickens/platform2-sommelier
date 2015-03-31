// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/server/dbus_service.h"

#include <string>

#include <dbus/bus.h>
#include <dbus/object_path.h>

#include "attestation/common/dbus_interface.h"

namespace attestation {

DBusService::DBusService(const scoped_refptr<dbus::Bus>& bus,
                         AttestationInterface* service)
    : dbus_object_(nullptr, bus, dbus::ObjectPath(kAttestationServicePath)),
      service_(service) {
}

void DBusService::Register(const CompletionAction& callback) {
  chromeos::dbus_utils::DBusInterface* dbus_interface =
      dbus_object_.AddOrGetInterface(kAttestationInterface);

  dbus_interface->AddSimpleMethodHandler(
      kCreateGoogleAttestedKey,
      base::Unretained(this),
      &DBusService::HandleCreateGoogleAttestedKey);

  dbus_object_.RegisterAsync(callback);
}

CreateGoogleAttestedKeyReply DBusService::HandleCreateGoogleAttestedKey(
    CreateGoogleAttestedKeyRequest request) {
  VLOG(1) << __func__;
  std::string certificate;
  std::string server_error_details;
  AttestationStatus status = service_->CreateGoogleAttestedKey(
    request.key_label(),
    request.key_type(),
    request.key_usage(),
    request.certificate_profile(),
    &certificate,
    &server_error_details);
  CreateGoogleAttestedKeyReply reply;
  reply.set_status(status);
  if (status == SUCCESS) {
    reply.set_certificate(certificate);
  } else if (status == REQUEST_DENIED_BY_CA) {
    reply.set_server_error(server_error_details);
  }
  return reply;
}

}  // namespace attestation
