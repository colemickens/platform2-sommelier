// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Asynchronous attestation tasks.

#include "attestation_task.h"

#include <string>

#include <chromeos/secure_blob.h>

#include "attestation.h"

using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

AttestationTask::AttestationTask(AttestationTaskObserver* observer,
                                 Attestation* attestation)
    : MountTask(observer, NULL),
      attestation_(attestation) {
}

AttestationTask::~AttestationTask() {}

CreateEnrollRequestTask::CreateEnrollRequestTask(
    AttestationTaskObserver* observer,
    Attestation* attestation,
    Attestation::PCAType pca_type)
    : AttestationTask(observer, attestation), pca_type_(pca_type) {
}

CreateEnrollRequestTask::~CreateEnrollRequestTask() {}

void CreateEnrollRequestTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    SecureBlob pca_request;
    bool status = attestation_->CreateEnrollRequest(pca_type_, &pca_request);
    result()->set_return_status(status);
    result()->set_return_data(pca_request);
  }
  Notify();
}

EnrollTask::EnrollTask(AttestationTaskObserver* observer,
                       Attestation* attestation,
                       Attestation::PCAType pca_type,
                       const SecureBlob& pca_response)
    : AttestationTask(observer, attestation),
      pca_type_(pca_type),
      pca_response_(pca_response) {
}

EnrollTask::~EnrollTask() {}

void EnrollTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    bool status = attestation_->Enroll(pca_type_, pca_response_);
    result()->set_return_status(status);
  }
  Notify();
}

CreateCertRequestTask::CreateCertRequestTask(AttestationTaskObserver* observer,
                                             Attestation* attestation,
                                             Attestation::PCAType pca_type,
                                             CertificateProfile profile,
                                             const string& username,
                                             const string& origin)
    : AttestationTask(observer, attestation),
      pca_type_(pca_type),
      profile_(profile),
      username_(username),
      origin_(origin) {
}

CreateCertRequestTask::~CreateCertRequestTask() {}

void CreateCertRequestTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    SecureBlob pca_request;
    bool status = attestation_->CreateCertRequest(pca_type_,
                                                  profile_,
                                                  username_,
                                                  origin_,
                                                  &pca_request);
    result()->set_return_status(status);
    result()->set_return_data(pca_request);
  }
  Notify();
}

FinishCertRequestTask::FinishCertRequestTask(AttestationTaskObserver* observer,
                                             Attestation* attestation,
                                             const SecureBlob& pca_response,
                                             bool is_user_specific,
                                             const string& username,
                                             const string& key_name)
    : AttestationTask(observer, attestation),
      pca_response_(pca_response),
      is_user_specific_(is_user_specific),
      username_(username),
      key_name_(key_name) {
}

FinishCertRequestTask::~FinishCertRequestTask() {}

void FinishCertRequestTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    SecureBlob cert;
    bool status = attestation_->FinishCertRequest(pca_response_,
                                                  is_user_specific_,
                                                  username_,
                                                  key_name_,
                                                  &cert);
    result()->set_return_status(status);
    result()->set_return_data(cert);
  }
  Notify();
}

SignChallengeTask::SignChallengeTask(AttestationTaskObserver* observer,
                                     Attestation* attestation,
                                     bool is_user_specific,
                                     const string& username,
                                     const string& key_name,
                                     const SecureBlob& challenge)
    : AttestationTask(observer, attestation),
      is_enterprise_(false),
      is_user_specific_(is_user_specific),
      username_(username),
      key_name_(key_name),
      challenge_(challenge) {
}

SignChallengeTask::SignChallengeTask(AttestationTaskObserver* observer,
                                     Attestation* attestation,
                                     bool is_user_specific,
                                     const string& username,
                                     const string& key_name,
                                     const string& domain,
                                     const SecureBlob& device_id,
                                     bool include_signed_public_key,
                                     const SecureBlob& challenge)
    : AttestationTask(observer, attestation),
      is_enterprise_(true),
      is_user_specific_(is_user_specific),
      username_(username),
      key_name_(key_name),
      domain_(domain),
      device_id_(device_id),
      include_signed_public_key_(include_signed_public_key),
      challenge_(challenge) {
}

SignChallengeTask::~SignChallengeTask() {}

void SignChallengeTask::Run() {
  result()->set_return_status(FALSE);
  if (!attestation_) {
    Notify();
    return;
  }
  SecureBlob response;
  bool status = false;
  if (is_enterprise_) {
    status = attestation_->SignEnterpriseChallenge(is_user_specific_,
                                                   username_,
                                                   key_name_,
                                                   domain_,
                                                   device_id_,
                                                   include_signed_public_key_,
                                                   challenge_,
                                                   &response);
  } else {
    status = attestation_->SignSimpleChallenge(is_user_specific_,
                                               username_,
                                               key_name_,
                                               challenge_,
                                               &response);
  }
  result()->set_return_status(status);
  result()->set_return_data(response);
  Notify();
}

RegisterKeyTask::RegisterKeyTask(AttestationTaskObserver* observer,
                                 Attestation* attestation,
                                 bool is_user_specific,
                                 const string& username,
                                 const string& key_name)
    : AttestationTask(observer, attestation),
      is_user_specific_(is_user_specific),
      username_(username),
      key_name_(key_name) {
}

RegisterKeyTask::~RegisterKeyTask() {}

void RegisterKeyTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    bool status = attestation_->RegisterKey(is_user_specific_,
                                            username_,
                                            key_name_);
    result()->set_return_status(status);
  }
  Notify();
}

}  // namespace cryptohome
