// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_REMOTE_ATTESTATION_H_
#define CRYPTOHOME_REMOTE_ATTESTATION_H_

#include "tpm.h"

namespace cryptohome {

class RemoteAttestation {
 public:
  RemoteAttestation(Tpm* tpm) : tpm_(tpm) {}
  virtual ~RemoteAttestation() {}

  // Returns true if the remote attestation data blobs already exist.
  virtual bool IsInitialized();

  // Initializes remote attestation data blobs if they do not already exist.
  virtual void Initialize();

  // Like Initialize(), but asynchronous.
  virtual void InitializeAsync();

 private:
  Tpm* tpm_;

  DISALLOW_COPY_AND_ASSIGN(RemoteAttestation);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_REMOTE_ATTESTATION_H_
