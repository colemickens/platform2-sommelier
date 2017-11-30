// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_DBUS_INTERFACE_H_
#define ATTESTATION_COMMON_DBUS_INTERFACE_H_

namespace attestation {

// TODO(namnguyen): Move to chromeos/system_api once we're ready.
constexpr char kAttestationInterface[] = "org.chromium.Attestation";
constexpr char kAttestationServicePath[] = "/org/chromium/Attestation";
constexpr char kAttestationServiceName[] = "org.chromium.Attestation";

// Methods exported by attestation.
constexpr char kCreateGoogleAttestedKey[] = "CreateGoogleAttestedKey";
constexpr char kGetKeyInfo[] = "GetKeyInfo";
constexpr char kGetEndorsementInfo[] = "GetEndorsementInfo";
constexpr char kGetAttestationKeyInfo[] = "GetAttestationKeyInfo";
constexpr char kActivateAttestationKey[] = "ActivateAttestationKey";
constexpr char kCreateCertifiableKey[] = "CreateCertifiableKey";
constexpr char kDecrypt[] = "Decrypt";
constexpr char kSign[] = "Sign";
constexpr char kRegisterKeyWithChapsToken[] = "RegisterKeyWithChapsToken";

}  // namespace attestation

#endif  // ATTESTATION_COMMON_DBUS_INTERFACE_H_
