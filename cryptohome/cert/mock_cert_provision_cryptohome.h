// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock for CryptohomeProxy.

#ifndef CRYPTOHOME_CERT_MOCK_CERT_PROVISION_CRYPTOHOME_H_
#define CRYPTOHOME_CERT_MOCK_CERT_PROVISION_CRYPTOHOME_H_

#include <string>

#include "cryptohome/cert/cert_provision_cryptohome.h"

namespace cert_provision {

class MockCryptohomeProxy : public CryptohomeProxy {
 public:
  MockCryptohomeProxy() = default;
  MOCK_METHOD0(Init, OpResult());
  MOCK_METHOD1(CheckIfPrepared, OpResult(bool *));
  MOCK_METHOD1(CheckIfEnrolled, OpResult(bool *));
  MOCK_METHOD2(CreateEnrollRequest, OpResult(PCAType, brillo::SecureBlob*));
  MOCK_METHOD2(ProcessEnrollResponse,
               OpResult(PCAType, const brillo::SecureBlob&));
  MOCK_METHOD3(CreateCertRequest,
               OpResult(PCAType, CertificateProfile, brillo::SecureBlob*));
  MOCK_METHOD3(ProcessCertResponse,
               OpResult(const std::string&,
                        const brillo::SecureBlob&,
                        brillo::SecureBlob*));
  MOCK_METHOD2(GetPublicKey,
               OpResult(const std::string&, brillo::SecureBlob*));
  MOCK_METHOD1(Register, OpResult(const std::string&));
};

}  // namespace cert_provision

#endif  // CRYPTOHOME_CERT_MOCK_CERT_PROVISION_CRYPTOHOME_H_
