// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "timberslide/mock_timberslide.h"
#include "timberslide/timberslide.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace timberslide {
namespace {

const char kSampleLogs[] =
    "[0.001000 UART initialized after sysjump]\n"
    "[1.000000 Sensor create: 0x0]\n";

const char kExpectedLogsWithUptime[] =
    "0101/000000.001000 [0.001000 UART initialized after sysjump]\n"
    "0101/000001.000000 [1.000000 Sensor create: 0x0]\n";

TEST(TimberslideTest, ProcessLogBuffer_GetEcUptimeSupported) {
  auto now = base::Time::FromDoubleT(1.0);
  NiceMock<MockTimberSlide> mock;
  EXPECT_CALL(mock, GetEcUptime).WillOnce([](int64_t* time) {
    *time = 1 * base::Time::kMillisecondsPerSecond;
    return true;
  });
  std::string ret = mock.ProcessLogBuffer(kSampleLogs, now);
  EXPECT_EQ(ret, kExpectedLogsWithUptime);
}

TEST(TimberslideTest, ProcessLogBuffer_GetEcUptimeNotSupported) {
  auto now = base::Time::FromDoubleT(1.0);
  NiceMock<MockTimberSlide> mock;
  EXPECT_CALL(mock, GetEcUptime).WillOnce(Return(false));
  std::string ret = mock.ProcessLogBuffer(kSampleLogs, now);
  EXPECT_EQ(ret, kSampleLogs);
}



}  // namespace
}  // namespace timberslide
