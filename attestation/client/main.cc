// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "attestation/client/dbus_proxy.h"
#include "attestation/common/attestation_ca.pb.h"
#include "attestation/common/interface.pb.h"

int main(int argc, char* argv[]) {
  std::unique_ptr<attestation::AttestationInterface> attestation(
      new attestation::DBusProxy());
  if (!attestation->Initialize()) {
    printf("Failed to initialize.\n");
  }
  std::string certificate;
  std::string server_error_details;
  attestation::AttestationStatus status = attestation->CreateGoogleAttestedKey(
    "test_key",
    attestation::KEY_TYPE_RSA,
    attestation::KEY_USAGE_SIGN,
    attestation::ENTERPRISE_MACHINE_CERTIFICATE,
    &certificate,
    &server_error_details);
  if (status == attestation::SUCCESS) {
    printf("Success!\n");
  } else {
    printf("Error occurred: %d.\n", status);
    if (!server_error_details.empty()) {
      printf("Server error details: %s\n", server_error_details.c_str());
    }
  }
  return status;
}
