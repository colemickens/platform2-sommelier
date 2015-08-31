// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_TPM_CONNECTION_H_
#define TPM_MANAGER_SERVER_TPM_CONNECTION_H_

#include <string>

#include <base/macros.h>
#include <trousers/scoped_tss_type.h>

namespace tpm_manager {

class TpmConnection {
 public:
  TpmConnection() = default;
  ~TpmConnection() = default;

  // This method returns a handle to the current Tpm context.
  // Note: this method still retains ownership of the context. If this class
  // is deleted, the context handle will be invalidated. Returns 0 on failure.
  TSS_HCONTEXT GetContext();

  // This method tries to get a handle to the TPM. Returns 0 on failure.
  TSS_HTPM GetTpm();

  // This method tries to get a handle to the TPM and with the given owner
  // password. Returns 0 on failure.
  TSS_HTPM GetTpmWithAuth(const std::string& owner_password);

 private:
  // This method connects to the Tpm. Returns true on success.
  bool ConnectContextIfNeeded();

  trousers::ScopedTssContext context_;

  DISALLOW_COPY_AND_ASSIGN(TpmConnection);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_CONNECTION_H_
