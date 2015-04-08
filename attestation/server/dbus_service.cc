// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/server/dbus_service.h"

#include <string>

#include <chromeos/bind_lambda.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>

#include "attestation/common/dbus_interface.h"

using chromeos::dbus_utils::DBusMethodResponse;

namespace attestation {

DBusService::DBusService(const scoped_refptr<dbus::Bus>& bus,
                         AttestationInterface* service)
    : dbus_object_(nullptr, bus, dbus::ObjectPath(kAttestationServicePath)),
      service_(service) {
}

void DBusService::Register(const CompletionAction& callback) {
  chromeos::dbus_utils::DBusInterface* dbus_interface =
      dbus_object_.AddOrGetInterface(kAttestationInterface);

  dbus_interface->AddMethodHandler(
      kCreateGoogleAttestedKey,
      base::Unretained(this),
      &DBusService::HandleCreateGoogleAttestedKey);

  dbus_object_.RegisterAsync(callback);
}

void DBusService::HandleCreateGoogleAttestedKey(
    scoped_ptr<DBusMethodResponse<const CreateGoogleAttestedKeyReply&>>
        response,
    const CreateGoogleAttestedKeyRequest& request) {
  VLOG(1) << __func__;
  // A callback that fills the reply protobuf and sends it.
  // CAUTION: This callback takes ownership of |response|.
  auto callback = [](
      scoped_ptr<DBusMethodResponse<const CreateGoogleAttestedKeyReply&>>
          response,
      AttestationStatus status,
      const std::string& certificate_chain,
      const std::string& server_error_details) {
    CreateGoogleAttestedKeyReply reply;
    reply.set_status(status);
    if (status == SUCCESS) {
      reply.set_certificate_chain(certificate_chain);
    } else if (status == REQUEST_DENIED_BY_CA) {
      reply.set_server_error(server_error_details);
    }
    response->Return(reply);
  };
  service_->CreateGoogleAttestedKey(request.key_label(),
                                    request.key_type(),
                                    request.key_usage(),
                                    request.certificate_profile(),
                                    base::Bind(callback,
                                               base::Passed(&response)));
}

}  // namespace attestation
