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

#ifndef TPM_MANAGER_COMMON_MOCK_TPM_NVRAM_INTERFACE_H_
#define TPM_MANAGER_COMMON_MOCK_TPM_NVRAM_INTERFACE_H_

#include <gmock/gmock.h>

#include "tpm_manager/common/tpm_nvram_interface.h"

namespace tpm_manager {

class MockTpmNvramInterface : public TpmNvramInterface {
 public:
  MockTpmNvramInterface();
  ~MockTpmNvramInterface() override;

  MOCK_METHOD(void,
              DefineSpace,
              (const DefineSpaceRequest&, const DefineSpaceCallback&),
              (override));
  MOCK_METHOD(void,
              DestroySpace,
              (const DestroySpaceRequest&, const DestroySpaceCallback&),
              (override));
  MOCK_METHOD(void,
              WriteSpace,
              (const WriteSpaceRequest&, const WriteSpaceCallback&),
              (override));
  MOCK_METHOD(void,
              ReadSpace,
              (const ReadSpaceRequest&, const ReadSpaceCallback&),
              (override));
  MOCK_METHOD(void,
              LockSpace,
              (const LockSpaceRequest&, const LockSpaceCallback&),
              (override));
  MOCK_METHOD(void,
              ListSpaces,
              (const ListSpacesRequest&, const ListSpacesCallback&),
              (override));
  MOCK_METHOD(void,
              GetSpaceInfo,
              (const GetSpaceInfoRequest&, const GetSpaceInfoCallback&),
              (override));
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_COMMON_MOCK_TPM_NVRAM_INTERFACE_H_
