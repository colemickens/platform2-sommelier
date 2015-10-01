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

#ifndef TPM_MANAGER_COMMON_TPM_MANAGER_INTERFACE_H_
#define TPM_MANAGER_COMMON_TPM_MANAGER_INTERFACE_H_

#include <base/callback.h>

#include "tpm_manager/common/dbus_interface.pb.h"
#include "tpm_manager/common/export.h"

namespace tpm_manager {

// This is the main TpmManager interface that is implemented by the proxies
// and services.
// TODO(usanghi): Break up the DBus interface (b/24659038).
class TPM_MANAGER_EXPORT TpmManagerInterface {
 public:
  virtual ~TpmManagerInterface() = default;

  // Performs initialization tasks. This method must be called before calling
  // any other method on this interface.
  virtual bool Initialize() = 0;

  // Processes a GetTpmStatusRequest and responds with a GetTpmStatusReply.
  using GetTpmStatusCallback = base::Callback<void(const GetTpmStatusReply&)>;
  virtual void GetTpmStatus(const GetTpmStatusRequest& request,
                            const GetTpmStatusCallback& callback) = 0;

  // Processes a TakeOwnershipRequest and responds with a TakeOwnershipReply.
  using TakeOwnershipCallback = base::Callback<void(const TakeOwnershipReply&)>;
  virtual void TakeOwnership(const TakeOwnershipRequest& request,
                             const TakeOwnershipCallback& callback) = 0;

  // Processes a DefineNvramRequest and responds with a DefineNvramReply.
  using DefineNvramCallback = base::Callback<void(const DefineNvramReply&)>;
  virtual void DefineNvram(const DefineNvramRequest& request,
                           const DefineNvramCallback& callback) = 0;

  // Processes a DestroyNvramRequest and responds with a DestroyNvramReply.
  using DestroyNvramCallback = base::Callback<void(const DestroyNvramReply&)>;
  virtual void DestroyNvram(const DestroyNvramRequest& request,
                            const DestroyNvramCallback& callback) = 0;

  // Processes a WriteNvramRequest and responds with a WriteNvramReply.
  using WriteNvramCallback = base::Callback<void(const WriteNvramReply&)>;
  virtual void WriteNvram(const WriteNvramRequest& request,
                          const WriteNvramCallback& callback) = 0;

  // Processes a ReadNvramRequest and responds with a ReadNvramReply.
  using ReadNvramCallback = base::Callback<void(const ReadNvramReply&)>;
  virtual void ReadNvram(const ReadNvramRequest& request,
                         const ReadNvramCallback& callback) = 0;

  // Processes a IsNvramDefinedRequest and responds with a IsNvramDefinedReply.
  using IsNvramDefinedCallback =
      base::Callback<void(const IsNvramDefinedReply&)>;
  virtual void IsNvramDefined(const IsNvramDefinedRequest& request,
                              const IsNvramDefinedCallback& callback) = 0;

  // Processes a IsNvramLockedRequest and responds with a IsNvramLockedReply.
  using IsNvramLockedCallback = base::Callback<void(const IsNvramLockedReply&)>;
  virtual void IsNvramLocked(const IsNvramLockedRequest& request,
                             const IsNvramLockedCallback& callback) = 0;

  // Processes a GetNvramSizeRequest and responds with a GetNvramSizeReply.
  using GetNvramSizeCallback = base::Callback<void(const GetNvramSizeReply&)>;
  virtual void GetNvramSize(const GetNvramSizeRequest& request,
                            const GetNvramSizeCallback& callback) = 0;
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_TPM_MANAGER_INTERFACE_H_
