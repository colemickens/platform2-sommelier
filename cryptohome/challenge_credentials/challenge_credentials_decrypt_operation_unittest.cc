// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the ChallengeCredentialsDecryptOperation class.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cryptohome/challenge_credentials/challenge_credentials_decrypt_operation.h"
#include "cryptohome/challenge_credentials/challenge_credentials_test_utils.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/mock_key_challenge_service.h"
#include "cryptohome/mock_signature_sealing_backend.h"
#include "cryptohome/mock_tpm.h"
#include "cryptohome/signature_sealing_backend.h"
#include "cryptohome/signature_sealing_backend_test_utils.h"
#include "cryptohome/username_passkey.h"

#include "key.pb.h"                    // NOLINT(build/include)
#include "rpc.pb.h"                    // NOLINT(build/include)
#include "signature_sealed_data.pb.h"  // NOLINT(build/include)

using brillo::Blob;
using brillo::BlobToString;
using brillo::CombineBlobs;
using testing::Return;
using testing::StrictMock;

namespace cryptohome {

using KeysetSignatureChallengeInfo =
    ChallengeCredentialsDecryptOperation::KeysetSignatureChallengeInfo;
using SealingAlgorithm = SignatureSealingBackend::Algorithm;

namespace {

ChallengePublicKeyInfo MakePublicKeyInfo(
    const Blob& set_public_key_spki_der,
    const std::vector<ChallengeSignatureAlgorithm>& key_algorithms) {
  ChallengePublicKeyInfo public_key_info;
  public_key_info.set_public_key_spki_der(
      BlobToString(set_public_key_spki_der));
  for (auto key_algorithm : key_algorithms)
    public_key_info.add_signature_algorithm(key_algorithm);
  return public_key_info;
}

KeysetSignatureChallengeInfo MakeFakeKeysetChallengeInfo(
    const Blob& public_key_spki_der,
    ChallengeSignatureAlgorithm salt_challenge_algorithm) {
  KeysetSignatureChallengeInfo keyset_challenge_info;
  keyset_challenge_info.set_public_key_spki_der(
      BlobToString(public_key_spki_der));
  *keyset_challenge_info.mutable_sealed_secret() =
      MakeFakeSignatureSealedData(public_key_spki_der);
  keyset_challenge_info.set_salt_signature_algorithm(salt_challenge_algorithm);
  return keyset_challenge_info;
}

// Base fixture class that provides some common constants, helpers and mocks for
// testing a single instance of ChallengeCredentialsDecryptOperation.
class ChallengeCredentialsDecryptOperationTestBase : public testing::Test {
 protected:
  // Set up mock for Tpm::GetSignatureSealingBackend().
  void PrepareSignatureSealingBackend() {
    EXPECT_CALL(tpm_, GetSignatureSealingBackend())
        .WillRepeatedly(Return(&sealing_backend_));
  }

  // Create an instance of ChallengeCredentialsDecryptOperation to be tested.
  void CreateOperation(
      const std::vector<ChallengeSignatureAlgorithm>& key_algorithms,
      ChallengeSignatureAlgorithm salt_challenge_algorithm) {
    DCHECK(!operation_);
    const ChallengePublicKeyInfo public_key_info =
        MakePublicKeyInfo(kPublicKeySpkiDer, key_algorithms);
    const KeysetSignatureChallengeInfo keyset_challenge_info =
        MakeFakeKeysetChallengeInfo(kPublicKeySpkiDer,
                                    salt_challenge_algorithm);
    operation_ = std::make_unique<ChallengeCredentialsDecryptOperation>(
        &challenge_service_, &tpm_, kDelegateBlob, kDelegateSecret, kUserEmail,
        public_key_info, kSalt, keyset_challenge_info,
        nullptr /* salt_signature */,
        MakeChallengeCredentialsDecryptResultWriter(&operation_result_));
  }

  void StartOperation() { operation_->Start(); }

  // Whether the tested operation completed with some result.
  bool has_result() const { return static_cast<bool>(operation_result_); }

  // Assert that the tested operation completed with a valid success result.
  void VerifySuccessfulResult() const {
    ASSERT_TRUE(operation_result_);
    VerifySuccessfulChallengeCredentialsDecryptResult(*operation_result_,
                                                      kUserEmail, kPasskey);
  }

  // Returns a helper object that aids mocking of the secret unsealing
  // functionality (SignatureSealingBackend::CreateUnsealingSession() et al.).
  std::unique_ptr<SignatureSealedUnsealingMocker> MakeUnsealingMocker(
      const std::vector<SealingAlgorithm>& key_sealing_algorithms,
      SealingAlgorithm unsealing_algorithm) {
    auto mocker =
        std::make_unique<SignatureSealedUnsealingMocker>(&sealing_backend_);
    mocker->set_public_key_spki_der(kPublicKeySpkiDer);
    mocker->set_key_algorithms(key_sealing_algorithms);
    mocker->set_delegate_blob(kDelegateBlob);
    mocker->set_delegate_secret(kDelegateSecret);
    mocker->set_chosen_algorithm(unsealing_algorithm);
    mocker->set_challenge_value(kUnsealingChallengeValue);
    mocker->set_challenge_signature(kUnsealingChallengeSignature);
    mocker->set_unsealed_secret(kUnsealedSecret);
    return mocker;
  }

  // Sets up an expectation that the salt challenge request will be issued via
  // |challenge_service_|.
  void ExpectSaltChallenge(
      ChallengeSignatureAlgorithm salt_challenge_algorithm) {
    salt_challenge_mock_controller_.ExpectSignatureChallenge(
        kUserEmail, kPublicKeySpkiDer, kSalt, salt_challenge_algorithm);
  }

  // Whether the salt challenge request has been started.
  bool is_salt_challenge_requested() const {
    return salt_challenge_mock_controller_.is_challenge_requested();
  }

  // Injects a simulated successful response for the currently running salt
  // challenge request.
  void SimulateSaltChallengeResponse() {
    salt_challenge_mock_controller_.SimulateSignatureChallengeResponse(
        kSaltSignature);
  }

  // Sets up an expectation that the secret unsealing challenge request will be
  // issued via |challenge_service_|.
  void ExpectUnsealingChallenge(
      ChallengeSignatureAlgorithm unsealing_challenge_algorithm) {
    unsealing_challenge_mock_controller_.ExpectSignatureChallenge(
        kUserEmail, kPublicKeySpkiDer, kUnsealingChallengeValue,
        unsealing_challenge_algorithm);
  }

  // Whether the secret unsealing challenge request has been started.
  bool is_unsealing_challenge_requested() const {
    return unsealing_challenge_mock_controller_.is_challenge_requested();
  }

  // Injects a simulated successful response for the currently running secret
  // unsealing challenge request.
  void SimulateUnsealingChallengeResponse() {
    unsealing_challenge_mock_controller_.SimulateSignatureChallengeResponse(
        kUnsealingChallengeSignature);
  }

 private:
  // Constants which are passed as fake data inputs to the
  // ChallengeCredentialsDecryptOperation operation:

  // Fake TPM delegate. It's supplied to the operation constructor. Then it's
  // verified to be passed into SignatureSealingBackend methods.
  const Blob kDelegateBlob{{1, 1, 1}};
  const Blob kDelegateSecret{{2, 2, 2}};
  // Fake user e-mail. It's supplied to the operation constructor. Then it's
  // verified to be passed alongside challenge requests made via
  // KeyChallengeService, and to be present in the resulting UsernamePasskey.
  const std::string kUserEmail = "foo@example.com";
  // Fake Subject Public Key Information of the challenged cryptographic key.
  // It's supplied to the operation as a field of both |public_key_info| and
  // |keyset_challenge_info| parameters. Then it's verified to be passed into
  // SignatureSealingBackend methods and to be used for challenge requests made
  // via KeyChallengeService.
  const Blob kPublicKeySpkiDer{{3, 3, 3}};
  // Fake salt value. It's supplied to the operation constructor. Then it's
  // verified to be used as the challenge value for one of requests made via
  // KeyChallengeService.
  const Blob kSalt{{4, 4, 4}};

  // Constants which are injected as fake data into intermediate steps of the
  // tested operation:

  // Fake signature of |kSalt| using the |salt_challenge_algorithm_| algorithm.
  // It's injected as a fake response to the salt challenge request made via
  // KeyChallengeService. Then it's implicitly verified to be used for the
  // generation of the passkey in the resulting UsernamePasskey - see the
  // |kPasskey| constant.
  const Blob kSaltSignature{{5, 5, 5}};
  // Fake challenge value for unsealing the secret. It's injected as a fake
  // value returned from SignatureSealingBackend::UnsealingSession. Then it's
  // verified to be used as the challenge value for one of requests made via
  // KeyChallengeService.
  const Blob kUnsealingChallengeValue{{6, 6, 6}};
  // Fake signature of |kUnsealingChallengeValue| using the
  // |unsealing_challenge_algorithm_| algorithm. It's injected as a fake
  // response to the unsealing challenge request made via KeyChallengeService.
  // Then it's verified to be passed to the Unseal() method of
  // SignatureSealingBackend::UnsealingSession.
  const Blob kUnsealingChallengeSignature{{7, 7, 7}};
  // Fake unsealed secret. It's injected as a fake result of the Unseal() method
  // of SignatureSealingBackend::UnsealingSession.
  const Blob kUnsealedSecret{{8, 8, 8}};

  // The expected passkey of the resulting UsernamePasskey returned from the
  // tested operation. Its value is derived from the injected fake data.
  const Blob kPasskey =
      CombineBlobs({kUnsealedSecret, CryptoLib::Sha256(kSaltSignature)});

  // Mock objects:

  StrictMock<MockTpm> tpm_;
  StrictMock<MockSignatureSealingBackend> sealing_backend_;
  StrictMock<MockKeyChallengeService> challenge_service_;
  KeyChallengeServiceMockController salt_challenge_mock_controller_{
      &challenge_service_};
  KeyChallengeServiceMockController unsealing_challenge_mock_controller_{
      &challenge_service_};

  // Result returned from the tested operation, or null if nothing yet.
  std::unique_ptr<ChallengeCredentialsDecryptResult> operation_result_;
  // The tested operation.
  std::unique_ptr<ChallengeCredentialsDecryptOperation> operation_;
};

// Base fixture class that uses a single algorithm for simplicity.
class ChallengeCredentialsDecryptOperationBasicTest
    : public ChallengeCredentialsDecryptOperationTestBase {
 protected:
  // Constants for the single algorithm to be used in this test.
  static constexpr ChallengeSignatureAlgorithm kChallengeAlgorithm =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;
  static constexpr SealingAlgorithm kSealingAlgorithm =
      SealingAlgorithm::kRsassaPkcs1V15Sha256;

  ChallengeCredentialsDecryptOperationBasicTest() {
    PrepareSignatureSealingBackend();
    CreateOperation({kChallengeAlgorithm} /* key_algorithms */,
                    kChallengeAlgorithm /* salt_challenge_algorithm */);
  }
};

}  // namespace

// Test success of the operation in scenario when the salt challenge response
// comes before the unsealing challenge response.
TEST_F(ChallengeCredentialsDecryptOperationBasicTest, Success) {
  ExpectSaltChallenge(kChallengeAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(
      kChallengeAlgorithm /* unsealing_challenge_algorithm */);
  MakeUnsealingMocker({kSealingAlgorithm} /* key_sealing_algorithms */,
                      kSealingAlgorithm /* unsealing_algorithm */)
      ->SetUpSuccessfulMock();

  StartOperation();
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());

  SimulateSaltChallengeResponse();
  EXPECT_FALSE(has_result());

  SimulateUnsealingChallengeResponse();
  VerifySuccessfulResult();
}

}  // namespace cryptohome
