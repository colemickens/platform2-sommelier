//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef TPM_MANAGER_SERVER_TPM_CONNECTION_H_
#define TPM_MANAGER_SERVER_TPM_CONNECTION_H_

#include <string>

#include <base/macros.h>
#include <trousers/scoped_tss_type.h>

#include "tpm_manager/common/tpm_manager.pb.h"

namespace tpm_manager {

class TpmConnection {
 public:
  enum ConnectionType {
    kConnectWithoutAuth,
    kConnectWithPassword,
    kConnectWithDelegate,
  };

  // Creates a TPM connection as a normal user w/o any auth.
  TpmConnection();

  // Creates a TPM connection on behalf of the owner with |owner_password|.
  explicit TpmConnection(const std::string& owner_password);

  // Creates a TPM connection on behalf of the owner with the owner delegate.
  explicit TpmConnection(const AuthDelegate& owner_delegate);

  ~TpmConnection() = default;

  // This method returns a handle to the current Tpm context.
  // Note: this method still retains ownership of the context. If this class
  // is deleted, the context handle will be invalidated. Returns 0 on failure.
  TSS_HCONTEXT GetContext();

  // This method tries to get a handle to the TPM. Returns 0 on failure.
  TSS_HTPM GetTpm();

 private:
  // This method connects to the Tpm. Returns true on success.
  bool ConnectContextIfNeeded();

  trousers::ScopedTssContext context_;

  const std::string owner_password_;
  const AuthDelegate owner_delegate_;
  const ConnectionType connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TpmConnection);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_CONNECTION_H_
