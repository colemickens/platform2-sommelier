// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_MOCK_ATTESTATION_INTERFACE_H_
#define ATTESTATION_COMMON_MOCK_ATTESTATION_INTERFACE_H_

#include <string>

#include <gmock/gmock.h>

#include "attestation/common/attestation_interface.h"

namespace attestation {

class MockAttestationInterface : public AttestationInterface {
 public:
  MockAttestationInterface() = default;
  virtual ~MockAttestationInterface() = default;

  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD2(CreateGoogleAttestedKey, void(
      const CreateGoogleAttestedKeyRequest&,
      const CreateGoogleAttestedKeyCallback&));
  MOCK_METHOD2(GetKeyInfo, void(const GetKeyInfoRequest&,
                                const GetKeyInfoCallback&));
  MOCK_METHOD2(GetEndorsementInfo, void(const GetEndorsementInfoRequest&,
                                        const GetEndorsementInfoCallback&));
  MOCK_METHOD2(GetAttestationKeyInfo,
               void(const GetAttestationKeyInfoRequest&,
                    const GetAttestationKeyInfoCallback&));
  MOCK_METHOD2(ActivateAttestationKey,
               void(const ActivateAttestationKeyRequest&,
                    const ActivateAttestationKeyCallback&));
  MOCK_METHOD2(CreateCertifiableKey, void(const CreateCertifiableKeyRequest&,
                                          const CreateCertifiableKeyCallback&));
  MOCK_METHOD2(Decrypt, void(const DecryptRequest&, const DecryptCallback&));
  MOCK_METHOD2(Sign, void(const SignRequest&, const SignCallback&));
  MOCK_METHOD2(RegisterKeyWithChapsToken,
               void(const RegisterKeyWithChapsTokenRequest&,
                    const RegisterKeyWithChapsTokenCallback&));
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_MOCK_ATTESTATION_INTERFACE_H_

