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

#ifndef TPM_MANAGER_COMMON_MOCK_TPM_OWNERSHIP_INTERFACE_H_
#define TPM_MANAGER_COMMON_MOCK_TPM_OWNERSHIP_INTERFACE_H_

#include <gmock/gmock.h>

#include "tpm_manager/common/tpm_ownership_interface.h"

namespace tpm_manager {

class MockTpmOwnershipInterface : public TpmOwnershipInterface {
 public:
  MockTpmOwnershipInterface();
  ~MockTpmOwnershipInterface() override;

  MOCK_METHOD(void,
              GetTpmStatus,
              (const GetTpmStatusRequest&, const GetTpmStatusCallback&),
              (override));
  MOCK_METHOD(void,
              GetDictionaryAttackInfo,
              (const GetDictionaryAttackInfoRequest&,
               const GetDictionaryAttackInfoCallback&),
              (override));
  MOCK_METHOD(void,
              ResetDictionaryAttackLock,
              (const ResetDictionaryAttackLockRequest&,
               const ResetDictionaryAttackLockCallback&),
              (override));
  MOCK_METHOD(void,
              TakeOwnership,
              (const TakeOwnershipRequest&, const TakeOwnershipCallback&),
              (override));
  MOCK_METHOD(void,
              RemoveOwnerDependency,
              (const RemoveOwnerDependencyRequest&,
               const RemoveOwnerDependencyCallback&),
              (override));
  MOCK_METHOD(void,
              ClearStoredOwnerPassword,
              (const ClearStoredOwnerPasswordRequest&,
               const ClearStoredOwnerPasswordCallback&),
              (override));
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_MOCK_TPM_OWNERSHIP_INTERFACE_H_
