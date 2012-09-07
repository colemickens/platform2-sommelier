// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stateful_recovery.h"

#include <gtest/gtest.h>

#include "mock_platform.h"

namespace cryptohome {
using std::string;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;

TEST(StatefulRecovery, ValidRequest) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, InvalidFlagFileContents) {
  MockPlatform platform;
  std::string flag_content = "0 hello";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  StatefulRecovery recovery(&platform);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UnreadableFlagFile) {
  MockPlatform platform;
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(Return(false));
  StatefulRecovery recovery(&platform);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UncopyableData) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

}  // namespace cryptohome
