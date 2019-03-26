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

#ifndef TPM_MANAGER_SERVER_MOCK_TPM_STATUS_H_
#define TPM_MANAGER_SERVER_MOCK_TPM_STATUS_H_

#include "tpm_manager/server/tpm_status.h"

#include <vector>

#include <gmock/gmock.h>

namespace tpm_manager {

class MockTpmStatus : public TpmStatus {
 public:
  MockTpmStatus();
  ~MockTpmStatus() override;

  MOCK_METHOD0(IsTpmEnabled, bool());
  MOCK_METHOD0(CheckAndNotifyIfTpmOwned, TpmOwnershipStatus());
  MOCK_METHOD4(GetDictionaryAttackInfo,
               bool(uint32_t* counter,
                    uint32_t* threshold,
                    bool* lockout,
                    uint32_t* seconds_remaining));
  MOCK_METHOD6(GetVersionInfo,
               bool(uint32_t* family,
                    uint64_t* spec_level,
                    uint32_t* manufacturer,
                    uint32_t* tpm_model,
                    uint64_t* firmware_version,
                    std::vector<uint8_t>* vendor_specific));
  MOCK_METHOD0(TestTpmWithDefaultOwnerPassword, bool());
  MOCK_METHOD0(MarkOwnerPasswordStateDirty, void());
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_MOCK_TPM_STATUS_H_
