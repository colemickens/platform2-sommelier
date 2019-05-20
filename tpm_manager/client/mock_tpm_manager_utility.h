// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_CLIENT_MOCK_TPM_MANAGER_UTILITY_H_
#define TPM_MANAGER_CLIENT_MOCK_TPM_MANAGER_UTILITY_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "tpm_manager/client/tpm_manager_utility.h"

namespace tpm_manager {

class TPM_MANAGER_EXPORT MockTpmManagerUtility : public TpmManagerUtility {
 public:
  MockTpmManagerUtility() {
    using ::testing::_;
    using ::testing::Return;
    ON_CALL(*this, Initialize()).WillByDefault(Return(true));
    ON_CALL(*this, TakeOwnership()).WillByDefault(Return(true));
    ON_CALL(*this, GetTpmStatus(_, _, _)).WillByDefault(Return(true));
    ON_CALL(*this, RemoveOwnerDependency(_)).WillByDefault(Return(true));
    ON_CALL(*this, GetDictionaryAttackInfo(_, _, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*this, ResetDictionaryAttackLock()).WillByDefault(Return(true));
    ON_CALL(*this, ReadSpace(_, _, _)).WillByDefault(Return(true));
  }
  ~MockTpmManagerUtility() override = default;

  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD0(TakeOwnership, bool());
  MOCK_METHOD3(GetTpmStatus, bool(bool*, bool*, LocalData*));
  MOCK_METHOD1(RemoveOwnerDependency, bool(const std::string&));
  MOCK_METHOD4(GetDictionaryAttackInfo, bool(int*, int*, bool*, int*));
  MOCK_METHOD0(ResetDictionaryAttackLock, bool());
  MOCK_METHOD3(ReadSpace, bool(uint32_t, bool, std::string*));
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_CLIENT_MOCK_TPM_MANAGER_UTILITY_H_
