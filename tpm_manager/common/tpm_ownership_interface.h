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

#ifndef TPM_MANAGER_COMMON_TPM_OWNERSHIP_INTERFACE_H_
#define TPM_MANAGER_COMMON_TPM_OWNERSHIP_INTERFACE_H_

#include <base/callback.h>

#include "tpm_manager/common/export.h"
#include "tpm_manager/common/tpm_manager.pb.h"

namespace tpm_manager {

// The command interface for TPM administration. Inherited by both IPC proxy
// and service classes. All methods are asynchronous because all TPM operations
// may take a long time to finish.
class TPM_MANAGER_EXPORT TpmOwnershipInterface {
 public:
  virtual ~TpmOwnershipInterface() = default;

  // Gets TPM status, which includes enabled, owned, passwords, etc. Processes
  // |request| and calls |callback| with a reply when the process is done. If
  // include_version_info is set in the request, TPM version info will be
  // included in the reply.
  using GetTpmStatusCallback = base::Callback<void(const GetTpmStatusReply&)>;
  virtual void GetTpmStatus(const GetTpmStatusRequest& request,
                            const GetTpmStatusCallback& callback) = 0;

  // Gets dictionary attack (DA) info. Processes |request| and calls |callback|
  // with a reply when the process is done.
  using GetDictionaryAttackInfoCallback =
      base::Callback<void(const GetDictionaryAttackInfoReply&)>;
  virtual void GetDictionaryAttackInfo(
      const GetDictionaryAttackInfoRequest& request,
      const GetDictionaryAttackInfoCallback& callback) = 0;

  // Processes a TakeOwnershipRequest and responds with a TakeOwnershipReply.
  using TakeOwnershipCallback = base::Callback<void(const TakeOwnershipReply&)>;
  virtual void TakeOwnership(const TakeOwnershipRequest& request,
                             const TakeOwnershipCallback& callback) = 0;

  // Processes a RemoveOwnerDependencyRequest and responds with a
  // RemoveOwnerDependencyReply.
  using RemoveOwnerDependencyCallback =
      base::Callback<void(const RemoveOwnerDependencyReply&)>;
  virtual void RemoveOwnerDependency(
      const RemoveOwnerDependencyRequest& request,
      const RemoveOwnerDependencyCallback& callback) = 0;

  // Processes a ClearStoredOwnerPasswordRequest and responds with a
  // ClearStoredOwnerPasswordReply.
  using ClearStoredOwnerPasswordCallback =
      base::Callback<void(const ClearStoredOwnerPasswordReply&)>;
  virtual void ClearStoredOwnerPassword(
      const ClearStoredOwnerPasswordRequest& request,
      const ClearStoredOwnerPasswordCallback& callback) = 0;
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_TPM_OWNERSHIP_INTERFACE_H_
