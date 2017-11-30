// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_TPM_INITIALIZER_H_
#define TPM_MANAGER_SERVER_TPM_INITIALIZER_H_

namespace tpm_manager {

// TpmInitializer performs initialization tasks on some kind of TPM device.
class TpmInitializer {
 public:
  TpmInitializer() = default;
  virtual ~TpmInitializer() = default;

  // Initializes a TPM and returns true on success. If the TPM is already
  // initialized, this method has no effect and succeeds. If the TPM is
  // partially initialized, e.g. the process was previously interrupted, then
  // the process picks up where it left off.
  virtual bool InitializeTpm() = 0;
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_INITIALIZER_H_
