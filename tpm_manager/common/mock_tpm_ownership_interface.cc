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

#include "tpm_manager/common/mock_tpm_ownership_interface.h"

using testing::_;
using testing::Invoke;
using testing::WithArgs;

namespace {

template <typename ReplyProtoType>
void RunCallback(const base::Callback<void(const ReplyProtoType&)>& callback) {
  ReplyProtoType empty_proto;
  callback.Run(empty_proto);
}

}  // namespace

namespace tpm_manager {

MockTpmOwnershipInterface::MockTpmOwnershipInterface() {
  ON_CALL(*this, GetTpmStatus(_, _))
      .WillByDefault(WithArgs<1>(Invoke(RunCallback<GetTpmStatusReply>)));
  ON_CALL(*this, GetDictionaryAttackInfo(_, _))
      .WillByDefault(
          WithArgs<1>(Invoke(RunCallback<GetDictionaryAttackInfoReply>)));
  ON_CALL(*this, ResetDictionaryAttackLock(_, _))
      .WillByDefault(
          WithArgs<1>(Invoke(RunCallback<ResetDictionaryAttackLockReply>)));
  ON_CALL(*this, TakeOwnership(_, _))
      .WillByDefault(WithArgs<1>(Invoke(RunCallback<TakeOwnershipReply>)));
  ON_CALL(*this, RemoveOwnerDependency(_, _))
      .WillByDefault(
          WithArgs<1>(Invoke(RunCallback<RemoveOwnerDependencyReply>)));
  ON_CALL(*this, ClearStoredOwnerPassword(_, _))
      .WillByDefault(
          WithArgs<1>(Invoke(RunCallback<ClearStoredOwnerPasswordReply>)));
}

MockTpmOwnershipInterface::~MockTpmOwnershipInterface() {}

}  // namespace tpm_manager
