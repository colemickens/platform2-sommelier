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

  MOCK_METHOD7(Initialize, void(Tpm*,
                                TpmInit*,
                                Platform*,
                                Crypto*,
                                InstallAttributes*,
                                const brillo::SecureBlob&,
                                bool));
  MOCK_METHOD0(IsPreparedForEnrollment, bool());
  MOCK_METHOD2(GetEnrollmentPreparations, bool(const brillo::SecureBlob&,
                                               brillo::SecureBlob));
  MOCK_METHOD0(IsEnrolled, bool());
  MOCK_METHOD1(IsFirstIdentityEnrolledWith, bool(Attestation::PCAType));
  MOCK_METHOD2(GetIdentityCertificates, bool(const brillo::SecureBlob&,
                                                  brillo::SecureBlob));
  MOCK_METHOD0(PrepareForEnrollment, void());
  MOCK_METHOD0(CacheEndorsementData, void());
  MOCK_METHOD0(PrepareForEnrollmentAsync, void());
  MOCK_METHOD1(Verify, bool(bool));
  MOCK_METHOD1(VerifyEK, bool(bool));
  MOCK_METHOD2(CreateEnrollRequest, bool(Attestation::PCAType,
                                         brillo::SecureBlob*));
  MOCK_METHOD2(Enroll, bool(Attestation::PCAType,
                            const brillo::SecureBlob&));
  MOCK_METHOD5(CreateCertRequest, bool(Attestation::PCAType,
                                       CertificateProfile,
                                       const std::string&,
                                       const std::string&,
                                       brillo::SecureBlob*));
  MOCK_METHOD5(FinishCertRequest, bool(const brillo::SecureBlob&,
                                       bool,
                                       const std::string&,
                                       const std::string&,
                                       brillo::SecureBlob*));
  MOCK_METHOD4(GetCertificateChain, bool(bool,
                                         const std::string&,
                                         const std::string&,
                                         brillo::SecureBlob*));
  MOCK_METHOD4(GetPublicKey, bool(bool,
                                  const std::string&,
                                  const std::string&,
                                  brillo::SecureBlob*));
  MOCK_METHOD3(DoesKeyExist, bool(bool,
                                  const std::string&,
                                  const std::string&));
  MOCK_METHOD8(SignEnterpriseChallenge, bool(bool,
                                             const std::string&,
                                             const std::string&,
                                             const std::string&,
                                             const brillo::SecureBlob&,
                                             bool,
                                             const brillo::SecureBlob&,
                                             brillo::SecureBlob*));
  MOCK_METHOD9(SignEnterpriseVaChallenge, bool(Attestation::VAType,
                                               bool,
                                               const std::string&,
                                               const std::string&,
                                               const std::string&,
                                               const brillo::SecureBlob&,
                                               bool,
                                               const brillo::SecureBlob&,
                                               brillo::SecureBlob*));
  MOCK_METHOD5(SignSimpleChallenge, bool(bool,
                                         const std::string&,
                                         const std::string&,
                                         const brillo::SecureBlob&,
                                         brillo::SecureBlob*));
  MOCK_METHOD4(RegisterKey, bool(bool,
                                 const std::string&,
                                 const std::string&,
                                 bool));
  MOCK_METHOD4(GetKeyPayload, bool(bool,
                                   const std::string&,
                                   const std::string&,
                                   brillo::SecureBlob*));
  MOCK_METHOD4(SetKeyPayload, bool(bool,
                                   const std::string&,
                                   const std::string&,
                                   const brillo::SecureBlob&));
  MOCK_METHOD3(DeleteKeysByPrefix, bool(bool,
                                        const std::string&,
                                        const std::string&));
  MOCK_METHOD1(GetEKInfo, bool(std::string*));
  MOCK_METHOD2(GetIdentityResetRequest, bool(const std::string&,
                                             brillo::SecureBlob*));
  MOCK_METHOD1(set_database_path, void(const char*));
  MOCK_METHOD2(set_enterprise_test_key, void(Attestation::VAType,
                                             RSA*));  // NOLINT "unnamed" param.
  MOCK_METHOD0(ThreadMain, void());
  MOCK_METHOD0(OnFinalized, void());
  MOCK_METHOD3(GetDelegateCredentials, bool(brillo::Blob*,
                                            brillo::Blob*,
                                            bool*));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_ATTESTATION_H_
