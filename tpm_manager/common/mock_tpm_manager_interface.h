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

#ifndef TPM_MANAGER_COMMON_MOCK_TPM_MANAGER_INTERFACE_H_
#define TPM_MANAGER_COMMON_MOCK_TPM_MANAGER_INTERFACE_H_

#include "tpm_manager/common/tpm_manager_interface.h"

#include <gmock/gmock.h>

namespace tpm_manager {

class MockTpmManagerInterface : public TpmManagerInterface {
 public:
  MockTpmManagerInterface();
  ~MockTpmManagerInterface() override;

  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD2(GetTpmStatus,
               void(const GetTpmStatusRequest& request,
                    const GetTpmStatusCallback& callback));
  MOCK_METHOD2(TakeOwnership,
               void(const TakeOwnershipRequest& request,
                    const TakeOwnershipCallback& callback));
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_MOCK_TPM_MANAGER_INTERFACE_H_
