// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_attestation.h"

namespace cryptohome {

bool RemoteAttestation::IsInitialized() {
  // TODO(dkrahn): Implement this.
  return false;
}

void RemoteAttestation::Initialize() {
  // If there is no TPM, we have no work to do.
  if (!tpm_)
    return;
  // TODO(dkrahn): Implement this.
  tpm_->RemoveOwnerDependency(Tpm::kRemoteAttestation);
}

void RemoteAttestation::InitializeAsync() {
  // TODO(dkrahn): Implement this.
  Initialize();
}

}
