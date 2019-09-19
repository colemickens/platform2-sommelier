// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "biod/fp_context_command_factory.h"
#include "biod/mock_cros_fp_device.h"

using testing::Return;

namespace biod {
namespace {

TEST(FpContextCommandFactory, Create_v1) {
  MockCrosFpDevice mock_cros_fp_device;
  EXPECT_CALL(mock_cros_fp_device, EcCmdVersionSupported)
      .Times(1)
      .WillOnce(Return(EcCmdVersionSupportStatus::SUPPORTED));

  auto cmd = FpContextCommandFactory::Create(&mock_cros_fp_device, "DEADBEEF");
  EXPECT_TRUE(cmd);
  EXPECT_EQ(cmd->Version(), 1);
}

TEST(FpContextCommandFactory, Create_v0) {
  MockCrosFpDevice mock_cros_fp_device;
  EXPECT_CALL(mock_cros_fp_device, EcCmdVersionSupported)
      .Times(1)
      .WillOnce(Return(EcCmdVersionSupportStatus::UNSUPPORTED));

  auto cmd = FpContextCommandFactory::Create(&mock_cros_fp_device, "DEADBEEF");
  EXPECT_TRUE(cmd);
  EXPECT_EQ(cmd->Version(), 0);
}

TEST(FpContextCommandFactory, Create_Version_Supported_Unknown) {
  MockCrosFpDevice mock_cros_fp_device;
  EXPECT_CALL(mock_cros_fp_device, EcCmdVersionSupported)
      .Times(1)
      .WillOnce(Return(EcCmdVersionSupportStatus::UNKNOWN));

  auto cmd = FpContextCommandFactory::Create(&mock_cros_fp_device, "DEADBEEF");
  EXPECT_TRUE(cmd);
  EXPECT_EQ(cmd->Version(), 0);
}

}  // namespace
}  // namespace biod
