//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef ATTESTATION_COMMON_MOCK_CRYPTO_UTILITY_H_
#define ATTESTATION_COMMON_MOCK_CRYPTO_UTILITY_H_

#include "attestation/common/crypto_utility.h"

#include <string>

#include <gmock/gmock.h>

namespace attestation {

class MockCryptoUtility : public CryptoUtility {
 public:
  MockCryptoUtility();
  ~MockCryptoUtility() override;

  MOCK_METHOD(bool, GetRandom, (size_t, std::string*), (const, override));

  MOCK_METHOD(bool, CreateSealedKey, (std::string*, std::string*), (override));

  MOCK_METHOD(bool,
              EncryptData,
              (const std::string&,
               const std::string&,
               const std::string&,
               std::string*),
              (override));

  MOCK_METHOD(bool,
              UnsealKey,
              (const std::string&, std::string*, std::string*),
              (override));

  MOCK_METHOD(bool,
              DecryptData,
              (const std::string&, const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              GetRSASubjectPublicKeyInfo,
              (const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              GetRSAPublicKey,
              (const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              EncryptIdentityCredential,
              (TpmVersion,
               const std::string&,
               const std::string&,
               const std::string&,
               EncryptedIdentityCredential*),
              (override));
  MOCK_METHOD(bool,
              DecryptIdentityCertificateForTpm2,
              (const std::string&, const EncryptedData&, std::string*),
              (override));
  MOCK_METHOD(bool,
              EncryptForUnbind,
              (const std::string&, const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              VerifySignature,
              (int, const std::string&, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool,
              VerifySignatureUsingHexKey,
              (int, const std::string&, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool,
              EncryptDataForGoogle,
              (const std::string&,
               const std::string&,
               const std::string&,
               EncryptedData*),
              (override));
  MOCK_METHOD(bool,
              CreateSPKAC,
              (const std::string&, const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              VerifyCertificate,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool,
              GetCertificateIssuerName,
              (const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              GetCertificateSubjectPublicKeyInfo,
              (const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              GetCertificatePublicKey,
              (const std::string&, std::string*),
              (override));
  MOCK_METHOD(bool,
              GetKeyDigest,
              (const std::string&, std::string*),
              (override));
  MOCK_METHOD(std::string,
              HmacSha256,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(std::string,
              HmacSha512,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(int, DefaultDigestAlgoForSingature, (), (override));
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_MOCK_CRYPTO_UTILITY_H_
