// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/client/dbus_proxy.h"

#include <chromeos/dbus/dbus_method_invoker.h>

#include "attestation/common/dbus_interface.h"

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

AttestationStatus DBusProxy::CreateGoogleAttestedKey(
    const std::string& key_label,
    KeyType key_type,
    KeyUsage key_usage,
    CertificateProfile certificate_profile,
    std::string* server_error_details,
    std::string* certificate) {
  attestation::CreateGoogleAttestedKeyRequest request;
  request.set_key_label(key_label);
  request.set_key_type(key_type);
  request.set_key_usage(key_usage);
  request.set_certificate_profile(certificate_profile);
  auto response = chromeos::dbus_utils::CallMethodAndBlock(
      object_proxy_,
      attestation::kAttestationInterface,
      attestation::kCreateGoogleAttestedKey,
      nullptr /* error */,
      request);
  if (!response) {
    return NOT_AVAILABLE;
  }
  attestation::CreateGoogleAttestedKeyReply reply;
  if (!chromeos::dbus_utils::ExtractMethodCallResults(
      response.get(),
      nullptr /* error */,
      &reply)) {
    return UNEXPECTED_DEVICE_ERROR;
  }
  *certificate = reply.certificate();
  *server_error_details = reply.server_error();
  return reply.status();
}

}  // namespace attestation
