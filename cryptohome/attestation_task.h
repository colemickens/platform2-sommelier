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

class CreateEnrollRequestTask : public AttestationTask {
 public:
  CreateEnrollRequestTask(AttestationTaskObserver* observer,
                          Attestation* attestation);
  virtual ~CreateEnrollRequestTask();

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateEnrollRequestTask);
};

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

class CreateCertRequestTask : public AttestationTask {
 public:
  CreateCertRequestTask(AttestationTaskObserver* observer,
                        Attestation* attestation,
                        bool is_cert_for_owner);
  virtual ~CreateCertRequestTask();

  virtual void Run();

 private:
  bool is_cert_for_owner_;

  DISALLOW_COPY_AND_ASSIGN(CreateCertRequestTask);
};

class FinishCertRequestTask : public AttestationTask {
 public:
  FinishCertRequestTask(AttestationTaskObserver* observer,
                        Attestation* attestation,
                        const SecureBlob& pca_response);
  virtual ~FinishCertRequestTask();

  virtual void Run();

 private:
  SecureBlob pca_response_;

  DISALLOW_COPY_AND_ASSIGN(FinishCertRequestTask);
};

}  // namespace cryptohome
