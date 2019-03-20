// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/client/tpm_manager_utility.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tuple>

#include "tpm_manager/common/mock_tpm_nvram_interface.h"
#include "tpm_manager/common/mock_tpm_ownership_interface.h"

namespace {

using ::testing::_;
using ::testing::ByRef;
using ::testing::NiceMock;
using ::testing::Test;
using ::testing::WithArg;

// testing::InvokeArgument<N> does not work with base::Callback, need to use
// |ACTION_TAMPLATE| along with predefined |args| tuple.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  std::get<k>(args).Run(p0);
}

}  // namespace

namespace tpm_manager {

class TpmManagerUtilityTest : public Test {
 protected:
  // Uses contructor and SetUp at the same time so we can take advantage of
  // both sytles of initialization.
  TpmManagerUtilityTest()
      : tpm_manager_utility_(&mock_tpm_owner_, &mock_tpm_nvram_) {
    // for the following ON_CALL setup, pass the fake replies by reference so we
    // can change the contained data dynamically in a test item.
    ON_CALL(mock_tpm_owner_, TakeOwnership(_, _))
        .WillByDefault(InvokeCallbackArgument<1>(ByRef(take_ownership_reply_)));
  }
  void SetUp() override { ASSERT_TRUE(tpm_manager_utility_.Initialize()); }

  NiceMock<tpm_manager::MockTpmOwnershipInterface> mock_tpm_owner_;
  NiceMock<tpm_manager::MockTpmNvramInterface> mock_tpm_nvram_;
  TpmManagerUtility tpm_manager_utility_;

  // fake replies from TpmManager
  tpm_manager::TakeOwnershipReply take_ownership_reply_;
};

TEST_F(TpmManagerUtilityTest, TakeOwnership) {
  take_ownership_reply_.set_status(tpm_manager::STATUS_SUCCESS);
  EXPECT_TRUE(tpm_manager_utility_.TakeOwnership());
}

TEST_F(TpmManagerUtilityTest, TakeOwnershipFail) {
  take_ownership_reply_.set_status(tpm_manager::STATUS_DEVICE_ERROR);
  EXPECT_FALSE(tpm_manager_utility_.TakeOwnership());
  take_ownership_reply_.set_status(tpm_manager::STATUS_NOT_AVAILABLE);
  EXPECT_FALSE(tpm_manager_utility_.TakeOwnership());
}

}  // namespace tpm_manager
