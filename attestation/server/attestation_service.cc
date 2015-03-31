// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/server/attestation_service.h"

namespace attestation {

bool AttestationService::Initialize() {
  return true;
}

AttestationStatus AttestationService::CreateGoogleAttestedKey(
    const std::string& key_label,
    KeyType key_type,
    KeyUsage key_usage,
    CertificateProfile certificate_profile,
    std::string* server_error_details,
    std::string* certificate) {
  return NOT_AVAILABLE;
}

}  // namespace attestation
