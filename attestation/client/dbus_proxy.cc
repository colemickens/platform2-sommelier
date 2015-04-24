// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/client/dbus_proxy.h"

#include <chromeos/bind_lambda.h>
#include <chromeos/dbus/dbus_method_invoker.h>

#include "attestation/common/dbus_interface.h"

namespace {

// Use a two minute timeout because TPM operations can take a long time and
// there may be a few of them queued up.
const int kDBusTimeoutMS = 120000;

}  // namespace

namespace attestation {

DBusProxy::DBusProxy() {}
DBusProxy::~DBusProxy() {
  if (bus_) {
    bus_->ShutdownAndBlock();
  }
}

bool DBusProxy::Initialize() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  object_proxy_ = bus_->GetObjectProxy(
      attestation::kAttestationServiceName,
      dbus::ObjectPath(attestation::kAttestationServicePath));
  return (object_proxy_ != nullptr);
}

void DBusProxy::CreateGoogleAttestedKey(
    const CreateGoogleAttestedKeyRequest& request,
    const CreateGoogleAttestedKeyCallback& callback) {
  auto on_error = [callback](chromeos::Error* error) {
    CreateGoogleAttestedKeyReply reply;
    reply.set_status(STATUS_NOT_AVAILABLE);
    callback.Run(reply);
  };
  chromeos::dbus_utils::CallMethodWithTimeout(
      kDBusTimeoutMS,
      object_proxy_,
      attestation::kAttestationInterface,
      attestation::kCreateGoogleAttestedKey,
      callback,
      base::Bind(on_error),
      request);
}

}  // namespace attestation
