// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "biod/cros_fp_device.h"
#include "biod/mock_biod_metrics.h"
#include "biod/mock_cros_fp_device.h"

namespace biod {
namespace {

class MockEcCommandInterface : public EcCommandInterface {
 public:
  MOCK_METHOD(bool, Run, (int fd));
  MOCK_METHOD(uint32_t, Version, (), (const));
  MOCK_METHOD(uint32_t, Command, (), (const));
};

class MockEcCommandFactory : public EcCommandFactoryInterface {
 public:
  std::unique_ptr<EcCommandInterface> FpContextCommand(
      CrosFpDeviceInterface* cros_fp, const std::string& user_id) override {
    auto cmd = std::make_unique<MockEcCommandInterface>();
    EXPECT_CALL(*cmd, Run).WillOnce(testing::Return(true));
    return cmd;
  }
};

class CrosFpDevice_ResetContext : public testing::Test {
 public:
  class MockCrosFpDevice : public CrosFpDevice {
   public:
    using CrosFpDevice::CrosFpDevice;
    MOCK_METHOD(bool, GetFpMode, (FpMode * mode));
    MOCK_METHOD(bool, SetContext, (std::string user_id));
  };
  metrics::MockBiodMetrics mock_biod_metrics;
  MockCrosFpDevice mock_cros_fp_device{
      CrosFpDevice::MkbpCallback(), &mock_biod_metrics,
      std::make_unique<MockEcCommandFactory>()};
};

TEST_F(CrosFpDevice_ResetContext, Success) {
  EXPECT_CALL(mock_cros_fp_device, GetFpMode)
      .Times(1)
      .WillOnce([](FpMode* mode) {
        *mode = FpMode(FpMode::Mode::kNone);
        return true;
      });
  EXPECT_CALL(mock_cros_fp_device, SetContext(std::string())).Times(1);
  EXPECT_CALL(mock_biod_metrics,
              SendResetContextMode(FpMode(FpMode::Mode::kNone)));

  mock_cros_fp_device.ResetContext();
}

TEST_F(CrosFpDevice_ResetContext, WrongMode) {
  EXPECT_CALL(mock_cros_fp_device, GetFpMode)
      .Times(1)
      .WillOnce([](FpMode* mode) {
        *mode = FpMode(FpMode::Mode::kMatch);
        return true;
      });
  EXPECT_CALL(mock_cros_fp_device, SetContext(std::string())).Times(1);
  EXPECT_CALL(mock_biod_metrics,
              SendResetContextMode(FpMode(FpMode::Mode::kMatch)));

  mock_cros_fp_device.ResetContext();
}

TEST_F(CrosFpDevice_ResetContext, Failure) {
  EXPECT_CALL(mock_cros_fp_device, GetFpMode)
      .Times(1)
      .WillOnce([](FpMode* mode) { return false; });
  EXPECT_CALL(mock_cros_fp_device, SetContext(std::string())).Times(1);
  EXPECT_CALL(mock_biod_metrics,
              SendResetContextMode(FpMode(FpMode::Mode::kModeInvalid)));

  mock_cros_fp_device.ResetContext();
}

}  // namespace
}  // namespace biod
