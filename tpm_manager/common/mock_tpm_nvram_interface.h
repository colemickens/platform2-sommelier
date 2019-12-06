// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
