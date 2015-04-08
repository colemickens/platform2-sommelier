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
    const std::string& key_label,
    KeyType key_type,
    KeyUsage key_usage,
    CertificateProfile certificate_profile,
    const base::Callback<CreateGoogleAttestedKeyCallback>& callback) {
  attestation::CreateGoogleAttestedKeyRequest request;
  request.set_key_label(key_label);
  request.set_key_type(key_type);
  request.set_key_usage(key_usage);
  request.set_certificate_profile(certificate_profile);
  auto on_success = [callback](
      const attestation::CreateGoogleAttestedKeyReply& reply) {
    callback.Run(reply.status(),
                 reply.certificate_chain(),
                 reply.server_error());
  };
  auto on_error = [callback](chromeos::Error* error) {
    callback.Run(NOT_AVAILABLE, std::string(), std::string());
  };
  chromeos::dbus_utils::CallMethodWithTimeout(
      kDBusTimeoutMS,
      object_proxy_,
      attestation::kAttestationInterface,
      attestation::kCreateGoogleAttestedKey,
      base::Bind(on_success),
      base::Bind(on_error),
      request);
}

}  // namespace attestation
