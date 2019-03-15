// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/challenge_credentials/challenge_credentials_operation.h"

#include <iterator>

#include <base/bind.h>
#include <base/logging.h>
#include <base/no_destructor.h>

#include "cryptohome/key_challenge_service.h"

#include "rpc.pb.h"  // NOLINT(build/include)

using brillo::Blob;
using brillo::BlobFromString;
using brillo::BlobToString;

namespace cryptohome {

namespace {

// The constant prefix for the salt for challenge-protected credentials (see the
// comment on ChallengeCredentialsOperation::GetSaltConstantPrefix() for
// details).
//
// For extra safety, this constant is made longer than 64 bytes and is
// terminated with a null character, following the safety measures made in TLS
// 1.3: https://tools.ietf.org/html/draft-ietf-tls-tls13-23#section-4.4.3 .
constexpr char kSaltConstantPrefix[] =
    "Chrome OS challenge credentials salt Chrome OS challenge credentials "
    "salt\0";
static_assert(arraysize(kSaltConstantPrefix) > 64,
              "The salt prefix is too short");
static_assert(!kSaltConstantPrefix[arraysize(kSaltConstantPrefix) - 1],
              "The salt prefix must terminate with a null character");

// Is called when a response is received for the sent signature challenge
// request.
void OnKeySignatureChallengeResponse(
    const ChallengeCredentialsOperation::KeySignatureChallengeCallback&
        response_callback,
    std::unique_ptr<KeyChallengeResponse> response) {
  if (!response) {
    LOG(ERROR) << "Signature challenge request failed";
    response_callback.Run(nullptr /* signature */);
    return;
  }
  if (!response->has_signature_response_data() ||
      !response->signature_response_data().has_signature()) {
    LOG(ERROR) << "Signature challenge response is invalid";
    response_callback.Run(nullptr /* signature */);
    return;
  }
  response_callback.Run(std::make_unique<Blob>(
      BlobFromString(response->signature_response_data().signature())));
}

}  // namespace

// static
const brillo::Blob& ChallengeCredentialsOperation::GetSaltConstantPrefix() {
  static const base::NoDestructor<brillo::Blob> salt_constant_prefix(
      BlobFromString(std::string(std::begin(kSaltConstantPrefix),
                                 std::end(kSaltConstantPrefix))));
  // Verify that we correctly converted the static character constant, without
  // losing the trailing null character.
  CHECK(!salt_constant_prefix->back());
  return *salt_constant_prefix;
}

ChallengeCredentialsOperation::~ChallengeCredentialsOperation() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

ChallengeCredentialsOperation::ChallengeCredentialsOperation(
    KeyChallengeService* key_challenge_service)
    : key_challenge_service_(key_challenge_service) {}

void ChallengeCredentialsOperation::MakeKeySignatureChallenge(
    const std::string& account_id,
    const Blob& public_key_spki_der,
    const Blob& data_to_sign,
    ChallengeSignatureAlgorithm signature_algorithm,
    const KeySignatureChallengeCallback& response_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  AccountIdentifier account_identifier;
  account_identifier.set_account_id(account_id);

  KeyChallengeRequest challenge_request;
  challenge_request.set_challenge_type(
      KeyChallengeRequest::CHALLENGE_TYPE_SIGNATURE);
  SignatureKeyChallengeRequestData& challenge_request_data =
      *challenge_request.mutable_signature_request_data();
  challenge_request_data.set_data_to_sign(BlobToString(data_to_sign));
  challenge_request_data.set_public_key_spki_der(
      BlobToString(public_key_spki_der));
  challenge_request_data.set_signature_algorithm(signature_algorithm);

  key_challenge_service_->ChallengeKey(
      account_identifier, challenge_request,
      base::Bind(&OnKeySignatureChallengeResponse, response_callback));
}

}  // namespace cryptohome
