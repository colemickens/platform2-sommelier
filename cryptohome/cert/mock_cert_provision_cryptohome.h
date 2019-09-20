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
  MOCK_METHOD(OpResult, Init, (), (override));
  MOCK_METHOD(OpResult, CheckIfPrepared, (bool*), (override));
  MOCK_METHOD(OpResult, CheckIfEnrolled, (bool*), (override));
  MOCK_METHOD(OpResult,
              CreateEnrollRequest,
              (PCAType, brillo::SecureBlob*),
              (override));
  MOCK_METHOD(OpResult,
              ProcessEnrollResponse,
              (PCAType, const brillo::SecureBlob&),
              (override));
  MOCK_METHOD(OpResult,
              CreateCertRequest,
              (PCAType, CertificateProfile, brillo::SecureBlob*),
              (override));
  MOCK_METHOD(OpResult,
              ProcessCertResponse,
              (const std::string&,
               const brillo::SecureBlob&,
               brillo::SecureBlob*),
              (override));
  MOCK_METHOD(OpResult,
              GetPublicKey,
              (const std::string&, brillo::SecureBlob*),
              (override));
  MOCK_METHOD(OpResult, Register, (const std::string&), (override));
};

}  // namespace cert_provision

#endif  // CRYPTOHOME_CERT_MOCK_CERT_PROVISION_CRYPTOHOME_H_
