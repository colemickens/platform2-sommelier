// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Asynchronous attestation tasks.

#include "mount_task.h"

namespace cryptohome {

class Attestation;

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
                          Attestation* attestation);
  virtual ~CreateEnrollRequestTask();

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateEnrollRequestTask);
};

// An asynchronous task for Attestation::Enroll().
class EnrollTask : public AttestationTask {
 public:
  EnrollTask(AttestationTaskObserver* observer,
             Attestation* attestation,
             const SecureBlob& pca_response);
  virtual ~EnrollTask();

  virtual void Run();

 private:
  SecureBlob pca_response_;

  DISALLOW_COPY_AND_ASSIGN(EnrollTask);
};

// An asynchronous task for Attestation::CreateCertRequest().
class CreateCertRequestTask : public AttestationTask {
 public:
  CreateCertRequestTask(AttestationTaskObserver* observer,
                        Attestation* attestation,
                        bool include_stable_id,
                        bool include_device_state);
  virtual ~CreateCertRequestTask();

  virtual void Run();

 private:
  bool include_stable_id_;
  bool include_device_state_;

  DISALLOW_COPY_AND_ASSIGN(CreateCertRequestTask);
};

// An asynchronous task for Attestation::FinishCertRequest().
class FinishCertRequestTask : public AttestationTask {
 public:
  FinishCertRequestTask(AttestationTaskObserver* observer,
                        Attestation* attestation,
                        const SecureBlob& pca_response,
                        bool is_user_specific,
                        const std::string& key_name);
  virtual ~FinishCertRequestTask();

  virtual void Run();

 private:
  SecureBlob pca_response_;
  bool is_user_specific_;
  std::string key_name_;

  DISALLOW_COPY_AND_ASSIGN(FinishCertRequestTask);
};

}  // namespace cryptohome
