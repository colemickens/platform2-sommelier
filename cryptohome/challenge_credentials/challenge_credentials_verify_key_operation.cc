// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/challenge_credentials/challenge_credentials_verify_key_operation.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/optional.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "cryptohome/tpm.h"

using brillo::Blob;
using brillo::BlobFromString;

namespace cryptohome {

namespace {

// Size of the verification challenge.
constexpr int kChallengeByteCount = 20;

// Returns the signature algorithm to be used for the verification.
base::Optional<ChallengeSignatureAlgorithm> ChooseChallengeAlgorithm(
    const ChallengePublicKeyInfo& public_key_info) {
  DCHECK(public_key_info.signature_algorithm_size());
  base::Optional<ChallengeSignatureAlgorithm> currently_chosen_algorithm;
  // Respect the input's algorithm prioritization, with the exception of
  // considering SHA-1 as the least preferred option.
  for (int index = 0; index < public_key_info.signature_algorithm_size();
       ++index) {
    currently_chosen_algorithm = public_key_info.signature_algorithm(index);
    if (*currently_chosen_algorithm != CHALLENGE_RSASSA_PKCS1_V1_5_SHA1)
      break;
  }
  return currently_chosen_algorithm;
}

int GetOpenSslSignatureAlgorithmNid(ChallengeSignatureAlgorithm algorithm) {
  switch (algorithm) {
    case CHALLENGE_RSASSA_PKCS1_V1_5_SHA1:
      return NID_sha1;
    case CHALLENGE_RSASSA_PKCS1_V1_5_SHA256:
      return NID_sha256;
    case CHALLENGE_RSASSA_PKCS1_V1_5_SHA384:
      return NID_sha384;
    case CHALLENGE_RSASSA_PKCS1_V1_5_SHA512:
      return NID_sha512;
  }
  return 0;
}

bool IsValidSignature(const Blob& public_key_spki_der,
                      ChallengeSignatureAlgorithm algorithm,
                      const Blob& input,
                      const Blob& signature) {
  const int openssl_algorithm_nid = GetOpenSslSignatureAlgorithmNid(algorithm);
  if (!openssl_algorithm_nid) {
    LOG(ERROR)
        << "Unknown challenge algorithm for challenge signature verification";
    return false;
  }
  const EVP_MD* const openssl_digest =
      EVP_get_digestbynid(openssl_algorithm_nid);
  if (!openssl_digest) {
    LOG(ERROR) << "Error obtaining digest for challenge signature verification";
    return false;
  }
  const unsigned char* asn1_ptr = public_key_spki_der.data();
  crypto::ScopedEVP_PKEY openssl_public_key(
      d2i_PUBKEY(NULL, &asn1_ptr, public_key_spki_der.size()));
  if (!openssl_public_key) {
    LOG(ERROR)
        << "Error loading public key for challenge signature verification";
    return false;
  }
  crypto::ScopedEVP_MD_CTX openssl_context(EVP_MD_CTX_new());
  if (!openssl_context) {
    LOG(ERROR) << "Error creating challenge signature verification context";
    return false;
  }
  if (!EVP_DigestVerifyInit(openssl_context.get(),
                            /*EVP_PKEY_CTX **pctx=*/nullptr, openssl_digest,
                            /*ENGINE *e=*/nullptr, openssl_public_key.get())) {
    LOG(ERROR) << "Failed to initialize challenge signature verification";
    return false;
  }
  if (!EVP_DigestVerifyUpdate(openssl_context.get(), input.data(),
                              input.size())) {
    LOG(ERROR)
        << "Failed to update digest for challenge signature verification";
    return false;
  }
  if (EVP_DigestVerifyFinal(openssl_context.get(), signature.data(),
                            signature.size()) != 1) {
    LOG(ERROR) << "Challenge signature verification failed";
    return false;
  }
  return true;
}

}  // namespace

ChallengeCredentialsVerifyKeyOperation::ChallengeCredentialsVerifyKeyOperation(
    KeyChallengeService* key_challenge_service,
    Tpm* tpm,
    const std::string& account_id,
    const KeyData& key_data,
    CompletionCallback completion_callback)
    : ChallengeCredentialsOperation(key_challenge_service),
      tpm_(tpm),
      account_id_(account_id),
      key_data_(key_data),
      completion_callback_(std::move(completion_callback)) {
  DCHECK_EQ(key_data.type(), KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
}

ChallengeCredentialsVerifyKeyOperation::
    ~ChallengeCredentialsVerifyKeyOperation() = default;

void ChallengeCredentialsVerifyKeyOperation::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!key_data_.challenge_response_key_size()) {
    LOG(ERROR) << "Missing challenge-response key information";
    Complete(&completion_callback_, /*is_key_valid=*/false);
    return;
  }
  if (key_data_.challenge_response_key_size() > 1) {
    LOG(ERROR)
        << "Using multiple challenge-response keys at once is unsupported";
    Complete(&completion_callback_, /*is_key_valid=*/false);
    return;
  }
  const ChallengePublicKeyInfo public_key_info =
      key_data_.challenge_response_key(0);
  const Blob public_key_spki_der =
      BlobFromString(public_key_info.public_key_spki_der());
  if (!public_key_info.signature_algorithm_size()) {
    LOG(ERROR) << "The key does not support any signature algorithm";
    Complete(&completion_callback_, /*is_key_valid=*/false);
    return;
  }
  const base::Optional<ChallengeSignatureAlgorithm> chosen_challenge_algorithm =
      ChooseChallengeAlgorithm(public_key_info);
  if (!chosen_challenge_algorithm) {
    LOG(ERROR) << "Failed to choose verification signature challenge algorithm";
    Complete(&completion_callback_, /*is_key_valid=*/false);
    return;
  }
  Blob challenge;
  if (!tpm_->GetRandomDataBlob(kChallengeByteCount, &challenge)) {
    LOG(ERROR)
        << "Failed to generate random bytes for the verification challenge";
    Complete(&completion_callback_, /*is_key_valid=*/false);
    return;
  }
  MakeKeySignatureChallenge(
      account_id_, public_key_spki_der, challenge, *chosen_challenge_algorithm,
      base::BindOnce(
          &ChallengeCredentialsVerifyKeyOperation::OnChallengeResponse,
          weak_ptr_factory_.GetWeakPtr(), public_key_spki_der,
          *chosen_challenge_algorithm, challenge));
}

void ChallengeCredentialsVerifyKeyOperation::Abort() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Complete(&completion_callback_, /*is_key_valid=*/false);
  // |this| can be already destroyed at this point.
}

void ChallengeCredentialsVerifyKeyOperation::OnChallengeResponse(
    const Blob& public_key_spki_der,
    ChallengeSignatureAlgorithm challenge_algorithm,
    const Blob& challenge,
    std::unique_ptr<Blob> challenge_response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!challenge_response) {
    LOG(ERROR) << "Verification signature challenge failed";
    Complete(&completion_callback_, /*is_key_valid=*/false);
    return;
  }
  if (!IsValidSignature(public_key_spki_der, challenge_algorithm, challenge,
                        *challenge_response)) {
    LOG(ERROR) << "Invalid signature for the verification challenge";
    Complete(&completion_callback_, /*is_key_valid=*/false);
    return;
  }
  Complete(&completion_callback_, /*is_key_valid=*/true);
}

}  // namespace cryptohome
