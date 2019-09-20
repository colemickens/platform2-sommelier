// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_ATTESTATION_H_
#define CRYPTOHOME_MOCK_ATTESTATION_H_

#include "cryptohome/attestation.h"

#include <memory>
#include <string>

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

namespace cryptohome {

class Platform;

class MockAttestation : public Attestation {
 public:
  MockAttestation(): Attestation() { }
  virtual ~MockAttestation() {}

  MOCK_METHOD(void,
              Initialize,
              (Tpm*,
               TpmInit*,
               Platform*,
               Crypto*,
               InstallAttributes*,
               const brillo::SecureBlob&,
               bool),
              (override));
  MOCK_METHOD(bool, IsPreparedForEnrollment, (), (override));
  MOCK_METHOD(bool, IsEnrolled, (), (override));
  MOCK_METHOD(void, PrepareForEnrollment, (), (override));
  MOCK_METHOD(void, CacheEndorsementData, (), (override));
  MOCK_METHOD(void, PrepareForEnrollmentAsync, (), (override));
  MOCK_METHOD(bool, Verify, (bool), (override));
  MOCK_METHOD(bool, VerifyEK, (bool), (override));
  MOCK_METHOD(bool,
              CreateEnrollRequest,
              (Attestation::PCAType, brillo::SecureBlob*),
              (override));
  MOCK_METHOD(bool,
              Enroll,
              (Attestation::PCAType, const brillo::SecureBlob&),
              (override));
  MOCK_METHOD(bool,
              CreateCertRequest,
              (Attestation::PCAType,
               CertificateProfile,
               const std::string&,
               const std::string&,
               brillo::SecureBlob*),
              (override));
  MOCK_METHOD(bool,
              FinishCertRequest,
              (const brillo::SecureBlob&,
               bool,
               const std::string&,
               const std::string&,
               brillo::SecureBlob*),
              (override));
  MOCK_METHOD(
      bool,
      GetCertificateChain,
      (bool, const std::string&, const std::string&, brillo::SecureBlob*),
      (override));
  MOCK_METHOD(
      bool,
      GetPublicKey,
      (bool, const std::string&, const std::string&, brillo::SecureBlob*),
      (override));
  MOCK_METHOD(bool,
              DoesKeyExist,
              (bool, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool,
              SignEnterpriseChallenge,
              (bool,
               const std::string&,
               const std::string&,
               const std::string&,
               const brillo::SecureBlob&,
               bool,
               const brillo::SecureBlob&,
               brillo::SecureBlob*),
              (override));
  MOCK_METHOD(bool,
              SignEnterpriseVaChallenge,
              (Attestation::VAType,
               bool,
               const std::string&,
               const std::string&,
               const std::string&,
               const brillo::SecureBlob&,
               bool,
               const brillo::SecureBlob&,
               const std::string&,
               brillo::SecureBlob*),
              (override));
  MOCK_METHOD(bool,
              SignSimpleChallenge,
              (bool,
               const std::string&,
               const std::string&,
               const brillo::SecureBlob&,
               brillo::SecureBlob*),
              (override));
  MOCK_METHOD(bool,
              RegisterKey,
              (bool, const std::string&, const std::string&, bool),
              (override));
  MOCK_METHOD(
      bool,
      GetKeyPayload,
      (bool, const std::string&, const std::string&, brillo::SecureBlob*),
      (override));
  MOCK_METHOD(
      bool,
      SetKeyPayload,
      (bool, const std::string&, const std::string&, const brillo::SecureBlob&),
      (override));
  MOCK_METHOD(bool,
              DeleteKeysByPrefix,
              (bool, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool, GetEKInfo, (std::string*), (override));
  MOCK_METHOD(bool,
              GetIdentityResetRequest,
              (const std::string&, brillo::SecureBlob*),
              (override));
  MOCK_METHOD(void, set_database_path, (const char*), (override));
  MOCK_METHOD(void, set_enterprise_test_key, (Attestation::VAType, RSA*));
  MOCK_METHOD(void, ThreadMain, (), (override));
  MOCK_METHOD(void, OnFinalized, (), (override));
  MOCK_METHOD(bool,
              GetDelegateCredentials,
              (brillo::Blob*, brillo::Blob*, bool*),
              (override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_ATTESTATION_H_
