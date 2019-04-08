// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the ChallengeCredentialsHelper class.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cryptohome/challenge_credentials/challenge_credentials_helper.h"
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
#include "vault_keyset.pb.h"           // NOLINT(build/include)

using brillo::Blob;
using brillo::BlobToString;
using brillo::CombineBlobs;
using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::StrictMock;
using testing::Values;

namespace cryptohome {

using KeysetSignatureChallengeInfo =
    SerializedVaultKeyset::SignatureChallengeInfo;

namespace {

KeyData MakeKeyData(
    const Blob& set_public_key_spki_der,
    const std::vector<ChallengeSignatureAlgorithm>& key_algorithms) {
  KeyData key_data;
  key_data.set_type(KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  ChallengePublicKeyInfo* const public_key_info =
      key_data.add_challenge_response_key();
  public_key_info->set_public_key_spki_der(
      BlobToString(set_public_key_spki_der));
  for (auto key_algorithm : key_algorithms)
    public_key_info->add_signature_algorithm(key_algorithm);
  return key_data;
}

KeysetSignatureChallengeInfo MakeFakeKeysetChallengeInfo(
    const Blob& public_key_spki_der,
    const Blob& salt,
    ChallengeSignatureAlgorithm salt_challenge_algorithm) {
  KeysetSignatureChallengeInfo keyset_challenge_info;
  keyset_challenge_info.set_public_key_spki_der(
      BlobToString(public_key_spki_der));
  *keyset_challenge_info.mutable_sealed_secret() =
      MakeFakeSignatureSealedData(public_key_spki_der);
  keyset_challenge_info.set_salt(BlobToString(salt));
  keyset_challenge_info.set_salt_signature_algorithm(salt_challenge_algorithm);
  return keyset_challenge_info;
}

// Base fixture class that provides some common constants, helpers and mocks for
// testing ChallengeCredentialsHelper.
class ChallengeCredentialsHelperTestBase : public testing::Test {
 protected:
  ChallengeCredentialsHelperTestBase()
      : challenge_credentials_helper_(&tpm_, kDelegateBlob, kDelegateSecret) {}

  void PrepareSignatureSealingBackend(bool enabled) {
    SignatureSealingBackend* const return_value =
        enabled ? &sealing_backend_ : nullptr;
    EXPECT_CALL(tpm_, GetSignatureSealingBackend())
        .WillRepeatedly(Return(return_value));
  }

  // Starts the asynchronous Decrypt() operation.  The result, once the
  // operation completes, will be stored in |decrypt_result|.
  void CallDecrypt(
      const std::vector<ChallengeSignatureAlgorithm>& key_algorithms,
      ChallengeSignatureAlgorithm salt_challenge_algorithm,
      const Blob& salt,
      std::unique_ptr<ChallengeCredentialsDecryptResult>* decrypt_result) {
    DCHECK(challenge_service_);
    const KeyData key_data = MakeKeyData(kPublicKeySpkiDer, key_algorithms);
    const KeysetSignatureChallengeInfo keyset_challenge_info =
        MakeFakeKeysetChallengeInfo(kPublicKeySpkiDer, salt,
                                    salt_challenge_algorithm);
    challenge_credentials_helper_.Decrypt(
        kUserEmail, key_data, keyset_challenge_info,
        std::move(challenge_service_),
        MakeChallengeCredentialsDecryptResultWriter(decrypt_result));
  }

  // Starts the Decrypt() operation without observing the challenge requests it
  // makes or its result. Intended to be used for testing the corner case of
  // starting an operation before the previous one is completed.
  void StartSurplusOperation() {
    // Use different parameters here, to avoid clashing with mocks set up for
    // the normal operation.
    constexpr ChallengeSignatureAlgorithm kLocalAlgorithm =
        CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;
    const Blob kLocalPublicKeySpkiDer =
        CombineBlobs({kPublicKeySpkiDer, Blob(1)});

    auto unsealing_mocker =
        MakeUnsealingMocker({kLocalAlgorithm} /* key_algorithms */,
                            kLocalAlgorithm /* unsealing_algorithm */);
    unsealing_mocker->set_public_key_spki_der(kLocalPublicKeySpkiDer);
    unsealing_mocker->SetUpUnsealingNotCalledMock();

    auto mock_key_challenge_service =
        std::make_unique<MockKeyChallengeService>();
    EXPECT_CALL(*mock_key_challenge_service, ChallengeKey(_, _, _))
        .Times(AnyNumber());
    const KeyData key_data = MakeKeyData(
        kLocalPublicKeySpkiDer, {kLocalAlgorithm} /* key_algorithms */);
    const KeysetSignatureChallengeInfo keyset_challenge_info =
        MakeFakeKeysetChallengeInfo(
            kLocalPublicKeySpkiDer, kSalt,
            kLocalAlgorithm /* salt_challenge_algorithm */);
    challenge_credentials_helper_.Decrypt(
        kUserEmail, key_data, keyset_challenge_info,
        std::move(mock_key_challenge_service),
        base::Bind([](std::unique_ptr<UsernamePasskey>) {}));
  }

  // Assert that the given decrypt operation result is a valid success result.
  void VerifySuccessfulDecryptResult(
      const ChallengeCredentialsDecryptResult& decrypt_result) const {
    VerifySuccessfulChallengeCredentialsDecryptResult(
        decrypt_result, kUserEmail,
        brillo::SecureBlob(kPasskey.begin(), kPasskey.end()));
  }

  // Returns a helper object that aids mocking of the secret unsealing
  // functionality (SignatureSealingBackend::CreateUnsealingSession() et al.).
  std::unique_ptr<SignatureSealedUnsealingMocker> MakeUnsealingMocker(
      const std::vector<ChallengeSignatureAlgorithm>& key_algorithms,
      ChallengeSignatureAlgorithm unsealing_algorithm) {
    auto mocker =
        std::make_unique<SignatureSealedUnsealingMocker>(&sealing_backend_);
    mocker->set_public_key_spki_der(kPublicKeySpkiDer);
    mocker->set_key_algorithms(key_algorithms);
    mocker->set_delegate_blob(kDelegateBlob);
    mocker->set_delegate_secret(kDelegateSecret);
    mocker->set_chosen_algorithm(unsealing_algorithm);
    mocker->set_challenge_value(kUnsealingChallengeValue);
    mocker->set_challenge_signature(kUnsealingChallengeSignature);
    mocker->set_secret_value(kUnsealedSecret);
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

  // Injects a simulated failure response for the currently running salt
  // challenge request.
  void SimulateSaltChallengeFailure() {
    salt_challenge_mock_controller_.SimulateFailureResponse();
  }

  // Sets up an expectation that the secret unsealing challenge request will be
  // issued via |challenge_service_|.
  void ExpectUnsealingChallenge(
      ChallengeSignatureAlgorithm unsealing_algorithm) {
    unsealing_challenge_mock_controller_.ExpectSignatureChallenge(
        kUserEmail, kPublicKeySpkiDer, kUnsealingChallengeValue,
        unsealing_algorithm);
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

  // Injects a simulated failure response for the currently running secret
  // unsealing challenge request.
  void SimulateUnsealingChallengeFailure() {
    unsealing_challenge_mock_controller_.SimulateFailureResponse();
  }

 protected:
  // Constants which are passed as fake data inputs to the
  // ChallengeCredentialsHelper methods:

  // Fake TPM delegate. It's supplied to the ChallengeCredentialsHelper
  // constructor. Then it's verified to be passed into SignatureSealingBackend
  // methods.
  const Blob kDelegateBlob{{1, 1, 1}};
  const Blob kDelegateSecret{{2, 2, 2}};
  // Fake user e-mail. It's supplied to the ChallengeCredentialsHelper operation
  // methods. Then it's verified to be passed alongside challenge requests made
  // via KeyChallengeService, and to be present in the resulting
  // UsernamePasskey.
  const std::string kUserEmail = "foo@example.com";
  // Fake Subject Public Key Information of the challenged cryptographic key.
  // It's supplied to the ChallengeCredentialsHelper operation methods as a
  // field of both |key_data| and |keyset_challenge_info| parameters. Then it's
  // verified to be passed into SignatureSealingBackend methods and to be used
  // for challenge requests made via KeyChallengeService.
  const Blob kPublicKeySpkiDer{{3, 3, 3}};
  // Fake salt value. It's supplied to the ChallengeCredentialsHelper operation
  // methods as a field of the |keyset_challenge_info| parameter. Then it's
  // verified to be used as the challenge value for one of requests made via
  // KeyChallengeService.
  const Blob kSalt = CombineBlobs(
      {ChallengeCredentialsOperation::GetSaltConstantPrefix(), Blob{4, 4, 4}});

  // Constants which are injected as fake data into intermediate steps of the
  // ChallengeCredentialsHelper operations:

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
  // |unsealing_algorithm_| algorithm. It's injected as a fake response to the
  // unsealing challenge request made via KeyChallengeService. Then it's
  // verified to be passed to the Unseal() method of
  // SignatureSealingBackend::UnsealingSession.
  const Blob kUnsealingChallengeSignature{{7, 7, 7}};
  // Fake unsealed secret. It's injected as a fake result of the Unseal() method
  // of SignatureSealingBackend::UnsealingSession.
  const Blob kUnsealedSecret{{8, 8, 8}};

  // The expected passkey of the resulting UsernamePasskey returned from the
  // ChallengeCredentialsHelper operations. Its value is derived from the
  // injected fake data.
  const Blob kPasskey =
      CombineBlobs({kUnsealedSecret, CryptoLib::Sha256(kSaltSignature)});

 private:
  // Mock objects:

  StrictMock<MockSignatureSealingBackend> sealing_backend_;
  StrictMock<MockTpm> tpm_;
  std::unique_ptr<StrictMock<MockKeyChallengeService>> challenge_service_ =
      std::make_unique<StrictMock<MockKeyChallengeService>>();
  KeyChallengeServiceMockController salt_challenge_mock_controller_{
      challenge_service_.get()};
  KeyChallengeServiceMockController unsealing_challenge_mock_controller_{
      challenge_service_.get()};

  // The tested instance.
  ChallengeCredentialsHelper challenge_credentials_helper_;
};

// Base fixture class that uses a single algorithm for simplicity.
class ChallengeCredentialsHelperSingleAlgorithmTestBase
    : public ChallengeCredentialsHelperTestBase {
 protected:
  // The single algorithm to be used in this test.
  static constexpr ChallengeSignatureAlgorithm kAlgorithm =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;
};

// Base fixture class that uses a single algorithm and have the sealing backend
// available.
class ChallengeCredentialsHelperBasicTest
    : public ChallengeCredentialsHelperSingleAlgorithmTestBase {
 protected:
  // The single algorithm to be used in this test.
  static constexpr ChallengeSignatureAlgorithm kAlgorithm =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;

  ChallengeCredentialsHelperBasicTest() {
    PrepareSignatureSealingBackend(true /* enabled */);
  }
};

}  // namespace

// Test failure of the Decrypt() operation due to the input salt being empty.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptFailureInSaltCheckEmpty) {
  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, Blob() /* salt */,
              &decrypt_result);
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);
}

// Test failure of the Decrypt() operation due to the input salt not starting
// with the expected constant prefix.
TEST_F(ChallengeCredentialsHelperBasicTest,
       DecryptFailureInSaltCheckNotPrefixed) {
  Blob salt = kSalt;
  salt[ChallengeCredentialsOperation::GetSaltConstantPrefix().size() - 1] ^= 1;
  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, salt, &decrypt_result);
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);
}

// Test failure of the Decrypt() operation due to the input salt containing
// nothing besides the expected constant prefix.
TEST_F(ChallengeCredentialsHelperBasicTest,
       DecryptFailureInSaltCheckNothingBesidesPrefix) {
  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */,
              ChallengeCredentialsOperation::GetSaltConstantPrefix() /* salt */,
              &decrypt_result);
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);
}

// Test success of the Decrypt() operation in scenario when the salt challenge
// response comes before the unsealing challenge response.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptSuccessSaltThenUnsealing) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpSuccessfulMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());

  SimulateSaltChallengeResponse();
  EXPECT_FALSE(decrypt_result);

  SimulateUnsealingChallengeResponse();
  ASSERT_TRUE(decrypt_result);
  VerifySuccessfulDecryptResult(*decrypt_result);
}

// Test success of the Decrypt() operation in scenario when the unsealing
// challenge response comes before the salt challenge response.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptSuccessUnsealingThenSalt) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpSuccessfulMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());

  SimulateUnsealingChallengeResponse();
  EXPECT_FALSE(decrypt_result);

  SimulateSaltChallengeResponse();
  ASSERT_TRUE(decrypt_result);
  VerifySuccessfulDecryptResult(*decrypt_result);
}

// Test failure of the Decrypt() operation due to failure of unsealing session
// creation.
TEST_F(ChallengeCredentialsHelperBasicTest,
       DecryptFailureInUnsealingSessionCreation) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpCreationFailingMock(true /* mock_repeatedly */);

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);

  // Responding to the salt challenge shouldn't have any effect.
  SimulateSaltChallengeResponse();
}

// Test failure of the Decrypt() operation due to failure of unsealing.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptFailureInUnsealing) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpUsealingFailingMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());
  EXPECT_FALSE(decrypt_result);

  SimulateUnsealingChallengeResponse();
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);

  // Responding to the salt challenge shouldn't have any effect.
  SimulateSaltChallengeResponse();
}

// Test failure of the Decrypt() operation due to failure of salt challenge
// request.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptFailureInSaltChallenge) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpUnsealingNotCalledMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());
  EXPECT_FALSE(decrypt_result);

  SimulateSaltChallengeFailure();
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);

  // Responding to the unsealing challenge shouldn't have any effect.
  SimulateUnsealingChallengeResponse();
}

// Test failure of the Decrypt() operation due to failure of unsealing challenge
// request.
TEST_F(ChallengeCredentialsHelperBasicTest,
       DecryptFailureInUnsealingChallenge) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpUnsealingNotCalledMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());
  EXPECT_FALSE(decrypt_result);

  SimulateUnsealingChallengeFailure();
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);

  // Responding to the salt challenge shouldn't have any effect.
  SimulateSaltChallengeResponse();
}

// Test failure of the Decrypt() operation due to its abortion before any of the
// challenges is completed.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptAbortionBeforeChallenges) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpUnsealingNotCalledMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());
  EXPECT_FALSE(decrypt_result);

  // Abort the first operation by starting a new one.
  StartSurplusOperation();
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);
}

// Test failure of the Decrypt() operation due to its abortion after the salt
// challenge completes.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptAbortionAfterSaltChallenge) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpUnsealingNotCalledMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());

  SimulateSaltChallengeResponse();
  EXPECT_FALSE(decrypt_result);

  // Abort the first operation by starting a new one.
  StartSurplusOperation();
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);
}

// Test failure of the Decrypt() operation due to its abortion after the
// unsealing completes.
TEST_F(ChallengeCredentialsHelperBasicTest, DecryptAbortionAfterUnsealing) {
  ExpectSaltChallenge(kAlgorithm /* salt_challenge_algorithm */);
  ExpectUnsealingChallenge(kAlgorithm /* unsealing_algorithm */);
  MakeUnsealingMocker({kAlgorithm} /* key_algorithms */,
                      kAlgorithm /* unsealing_algorithm */)
      ->SetUpSuccessfulMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());

  SimulateUnsealingChallengeResponse();
  EXPECT_FALSE(decrypt_result);

  // Abort the first operation by starting a new one.
  StartSurplusOperation();
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);
}

namespace {

// Tests with simulation of SignatureSealingBackend absence.
class ChallengeCredentialsHelperNoBackendTest
    : public ChallengeCredentialsHelperSingleAlgorithmTestBase {
 protected:
  ChallengeCredentialsHelperNoBackendTest() {
    PrepareSignatureSealingBackend(false /* enabled */);
  }
};

}  // namespace

// Test failure of the Decrypt() operation due to the absence of the sealing
// backend.
TEST_F(ChallengeCredentialsHelperNoBackendTest, DecryptFailure) {
  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt({kAlgorithm} /* key_algorithms */,
              kAlgorithm /* salt_challenge_algorithm */, kSalt,
              &decrypt_result);
  ASSERT_TRUE(decrypt_result);
  VerifyFailedChallengeCredentialsDecryptResult(*decrypt_result);
}

namespace {

// Test parameters for ChallengeCredentialsHelperAlgorithmsTest.
struct AlgorithmsTestParam {
  std::vector<ChallengeSignatureAlgorithm> key_algorithms;
  ChallengeSignatureAlgorithm salt_challenge_algorithm;
  ChallengeSignatureAlgorithm unsealing_algorithm;
};

// Tests various combinations of multiple algorithms.
class ChallengeCredentialsHelperAlgorithmsTest
    : public ChallengeCredentialsHelperTestBase,
      public testing::WithParamInterface<AlgorithmsTestParam> {
 protected:
  ChallengeCredentialsHelperAlgorithmsTest() {
    PrepareSignatureSealingBackend(true /* enabled */);
  }
};

}  // namespace

// Test success of the Decrypt() operation with the specified combination of
// algorithms.
TEST_P(ChallengeCredentialsHelperAlgorithmsTest, DecryptSuccess) {
  ExpectSaltChallenge(GetParam().salt_challenge_algorithm);
  ExpectUnsealingChallenge(GetParam().unsealing_algorithm);
  MakeUnsealingMocker(GetParam().key_algorithms, GetParam().unsealing_algorithm)
      ->SetUpSuccessfulMock();

  std::unique_ptr<ChallengeCredentialsDecryptResult> decrypt_result;
  CallDecrypt(GetParam().key_algorithms, GetParam().salt_challenge_algorithm,
              kSalt, &decrypt_result);
  EXPECT_TRUE(is_salt_challenge_requested());
  EXPECT_TRUE(is_unsealing_challenge_requested());

  SimulateSaltChallengeResponse();
  EXPECT_FALSE(decrypt_result);

  SimulateUnsealingChallengeResponse();
  ASSERT_TRUE(decrypt_result);
  VerifySuccessfulDecryptResult(*decrypt_result);
}

// Test that SHA-1 algorithms are the least preferred and chosen only if there's
// no other option.
INSTANTIATE_TEST_CASE_P(
    LowPriorityOfSha1,
    ChallengeCredentialsHelperAlgorithmsTest,
    Values(
        AlgorithmsTestParam{
            {CHALLENGE_RSASSA_PKCS1_V1_5_SHA1,
             CHALLENGE_RSASSA_PKCS1_V1_5_SHA256} /* key_algorithms */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA256 /* salt_challenge_algorithm */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA256 /* unsealing_algorithm */},
        AlgorithmsTestParam{
            {CHALLENGE_RSASSA_PKCS1_V1_5_SHA1} /* key_algorithms */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA1 /* salt_challenge_algorithm */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA1 /* unsealing_algorithm */}));

// Test prioritization of algorithms according to their order in the input.
INSTANTIATE_TEST_CASE_P(
    InputPrioritization,
    ChallengeCredentialsHelperAlgorithmsTest,
    Values(
        AlgorithmsTestParam{
            {CHALLENGE_RSASSA_PKCS1_V1_5_SHA256,
             CHALLENGE_RSASSA_PKCS1_V1_5_SHA512} /* key_algorithms */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA256 /* salt_challenge_algorithm */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA256 /* unsealing_algorithm */},
        AlgorithmsTestParam{
            {CHALLENGE_RSASSA_PKCS1_V1_5_SHA512,
             CHALLENGE_RSASSA_PKCS1_V1_5_SHA256} /* key_algorithms */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA512 /* salt_challenge_algorithm */,
            CHALLENGE_RSASSA_PKCS1_V1_5_SHA512 /* unsealing_algorithm */}));

}  // namespace cryptohome
