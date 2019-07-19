// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_key_challenge_service.h"

#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include "cryptohome/protobuf_test_utils.h"

using brillo::Blob;
using brillo::BlobToString;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::SaveArg;

namespace cryptohome {

MockKeyChallengeService::MockKeyChallengeService() = default;

MockKeyChallengeService::~MockKeyChallengeService() = default;

KeyChallengeServiceMockController::KeyChallengeServiceMockController(
    MockKeyChallengeService* mock_service)
    : mock_service_(mock_service) {}

KeyChallengeServiceMockController::~KeyChallengeServiceMockController() =
    default;

void KeyChallengeServiceMockController::ExpectSignatureChallenge(
    const std::string& expected_username,
    const Blob& expected_public_key_spki_der,
    const Blob& expected_challenge_value,
    ChallengeSignatureAlgorithm expected_signature_algorithm) {
  AccountIdentifier account_identifier;
  account_identifier.set_account_id(expected_username);

  KeyChallengeRequest challenge_request;
  challenge_request.set_challenge_type(
      KeyChallengeRequest::CHALLENGE_TYPE_SIGNATURE);
  SignatureKeyChallengeRequestData& request_data =
      *challenge_request.mutable_signature_request_data();
  request_data.set_data_to_sign(BlobToString(expected_challenge_value));
  request_data.set_public_key_spki_der(
      BlobToString(expected_public_key_spki_der));
  request_data.set_signature_algorithm(expected_signature_algorithm);

  EXPECT_CALL(*mock_service_,
              ChallengeKeyMovable(ProtobufEquals(account_identifier),
                                  ProtobufEquals(challenge_request), _))
      .WillOnce(Invoke(
          [this](const AccountIdentifier& account_id,
                 const KeyChallengeRequest& key_challenge_request,
                 KeyChallengeService::ResponseCallback* response_callback) {
            // Can't use SaveArg or SaveArgPointee here because
            // response_callback is move only.
            this->intercepted_response_callbacks_.push(
                std::move(*response_callback));
          }))
      .RetiresOnSaturation();
}

void KeyChallengeServiceMockController::SimulateSignatureChallengeResponse(
    const Blob& signature_value) {
  ASSERT_FALSE(intercepted_response_callbacks_.empty());

  auto response = std::make_unique<KeyChallengeResponse>();
  SignatureKeyChallengeResponseData& response_data =
      *response->mutable_signature_response_data();
  response_data.set_signature(BlobToString(signature_value));

  KeyChallengeService::ResponseCallback callback_to_run =
      std::move(intercepted_response_callbacks_.front());
  intercepted_response_callbacks_.pop();
  std::move(callback_to_run).Run(std::move(response));
}

void KeyChallengeServiceMockController::SimulateFailureResponse() {
  ASSERT_FALSE(intercepted_response_callbacks_.empty());
  KeyChallengeService::ResponseCallback callback_to_run =
      std::move(intercepted_response_callbacks_.front());
  intercepted_response_callbacks_.pop();
  std::move(callback_to_run).Run(nullptr /* response */);
}

}  // namespace cryptohome
