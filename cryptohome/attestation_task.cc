// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Asynchronous attestation tasks.

#include "attestation_task.h"

#include "attestation.h"

namespace cryptohome {

AttestationTask::AttestationTask(AttestationTaskObserver* observer,
                                 Attestation* attestation)
    : MountTask(observer, NULL, UsernamePasskey()),
      attestation_(attestation) {
}

AttestationTask::~AttestationTask() {}

CreateEnrollRequestTask::CreateEnrollRequestTask(
    AttestationTaskObserver* observer,
    Attestation* attestation)
    : AttestationTask(observer, attestation) {
}

CreateEnrollRequestTask::~CreateEnrollRequestTask() {}

void CreateEnrollRequestTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    SecureBlob pca_request;
    bool status = attestation_->CreateEnrollRequest(&pca_request);
    result()->set_return_status(status);
    result()->set_return_data(pca_request);
  }
  Notify();
}

EnrollTask::EnrollTask(AttestationTaskObserver* observer,
                       Attestation* attestation,
                       const SecureBlob& pca_response)
    : AttestationTask(observer, attestation),
      pca_response_(pca_response) {
}

EnrollTask::~EnrollTask() {}

void EnrollTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    bool status = attestation_->Enroll(pca_response_);
    result()->set_return_status(status);
  }
  Notify();
}

CreateCertRequestTask::CreateCertRequestTask(AttestationTaskObserver* observer,
                                             Attestation* attestation,
                                             bool is_cert_for_owner)
    : AttestationTask(observer, attestation),
      is_cert_for_owner_(is_cert_for_owner) {
}

CreateCertRequestTask::~CreateCertRequestTask() {}

void CreateCertRequestTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    SecureBlob pca_request;
    bool status = attestation_->CreateCertRequest(is_cert_for_owner_,
                                                  &pca_request);
    result()->set_return_status(status);
    result()->set_return_data(pca_request);
  }
  Notify();
}

FinishCertRequestTask::FinishCertRequestTask(AttestationTaskObserver* observer,
                                             Attestation* attestation,
                                             const SecureBlob& pca_response)
    : AttestationTask(observer, attestation),
      pca_response_(pca_response) {
}

FinishCertRequestTask::~FinishCertRequestTask() {}

void FinishCertRequestTask::Run() {
  result()->set_return_status(FALSE);
  if (attestation_) {
    SecureBlob cert;
    bool status = attestation_->FinishCertRequest(pca_response_, &cert);
    result()->set_return_status(status);
    result()->set_return_data(cert);
  }
  Notify();
}

}  // namespace cryptohome
