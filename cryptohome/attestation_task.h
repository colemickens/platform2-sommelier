// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Asynchronous attestation tasks.

#include "attestation.h"
#include "attestation.pb.h"
#include "mount_task.h"

namespace cryptohome {

typedef MountTaskObserver AttestationTaskObserver;

// This class represents an attestation task.  Inherit MountTask to reuse basic
// async code, especially the sequence counter.
class AttestationTask : public MountTask {
 public:
  AttestationTask(AttestationTaskObserver* observer, Attestation* attestation);
  virtual ~AttestationTask();

 protected:
  // The Attestation instance which will do the actual work.
  Attestation* attestation_;
};

// An asynchronous task for Attestation::CreateEnrollRequest().
class CreateEnrollRequestTask : public AttestationTask {
 public:
  CreateEnrollRequestTask(AttestationTaskObserver* observer,
                          Attestation* attestation,
                          Attestation::PCAType pca_type);
  virtual ~CreateEnrollRequestTask();

  virtual void Run();

 private:
  Attestation::PCAType pca_type_;

  DISALLOW_COPY_AND_ASSIGN(CreateEnrollRequestTask);
};

// An asynchronous task for Attestation::Enroll().
class EnrollTask : public AttestationTask {
 public:
  EnrollTask(AttestationTaskObserver* observer,
             Attestation* attestation,
             Attestation::PCAType pca_type,
             const SecureBlob& pca_response);
  virtual ~EnrollTask();

  virtual void Run();

 private:
  Attestation::PCAType pca_type_;
  SecureBlob pca_response_;

  DISALLOW_COPY_AND_ASSIGN(EnrollTask);
};

// An asynchronous task for Attestation::CreateCertRequest().
class CreateCertRequestTask : public AttestationTask {
 public:
  CreateCertRequestTask(AttestationTaskObserver* observer,
                        Attestation* attestation,
                        Attestation::PCAType pca_type,
                        CertificateProfile profile,
                        const std::string& username,
                        const std::string& origin);
  virtual ~CreateCertRequestTask();

  virtual void Run();

 private:
  Attestation::PCAType pca_type_;
  CertificateProfile profile_;
  std::string username_;
  std::string origin_;

  DISALLOW_COPY_AND_ASSIGN(CreateCertRequestTask);
};

// An asynchronous task for Attestation::FinishCertRequest().
class FinishCertRequestTask : public AttestationTask {
 public:
  FinishCertRequestTask(AttestationTaskObserver* observer,
                        Attestation* attestation,
                        const SecureBlob& pca_response,
                        bool is_user_specific,
                        const std::string& username,
                        const std::string& key_name);
  virtual ~FinishCertRequestTask();

  virtual void Run();

 private:
  SecureBlob pca_response_;
  bool is_user_specific_;
  std::string username_;
  std::string key_name_;

  DISALLOW_COPY_AND_ASSIGN(FinishCertRequestTask);
};

// An asynchronous task for Attestation::Sign*Challenge().
class SignChallengeTask : public AttestationTask {
 public:
  // Constructs a task for SignSimpleChallenge.
  SignChallengeTask(AttestationTaskObserver* observer,
                    Attestation* attestation,
                    bool is_user_specific,
                    const std::string& username,
                    const std::string& key_name,
                    const chromeos::SecureBlob& challenge);
  // Constructs a task for SignEnterpriseChallenge.
  SignChallengeTask(AttestationTaskObserver* observer,
                    Attestation* attestation,
                    bool is_user_specific,
                    const std::string& username,
                    const std::string& key_name,
                    const std::string& domain,
                    const chromeos::SecureBlob& device_id,
                    bool include_signed_public_key,
                    const chromeos::SecureBlob& challenge);
  virtual ~SignChallengeTask();

  virtual void Run();

 private:
  bool is_enterprise_;
  bool is_user_specific_;
  std::string username_;
  std::string key_name_;
  std::string domain_;
  chromeos::SecureBlob device_id_;
  bool include_signed_public_key_;
  chromeos::SecureBlob challenge_;

  DISALLOW_COPY_AND_ASSIGN(SignChallengeTask);
};

// An asynchronous task for Attestation::RegisterKey().
class RegisterKeyTask : public AttestationTask {
 public:
  RegisterKeyTask(AttestationTaskObserver* observer,
                        Attestation* attestation,
                        bool is_user_specific,
                        const std::string& username,
                        const std::string& key_name);
  virtual ~RegisterKeyTask();

  virtual void Run();

 private:
  bool is_user_specific_;
  std::string username_;
  std::string key_name_;

  DISALLOW_COPY_AND_ASSIGN(RegisterKeyTask);
};

}  // namespace cryptohome
