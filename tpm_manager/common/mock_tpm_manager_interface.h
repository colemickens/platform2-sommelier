// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
