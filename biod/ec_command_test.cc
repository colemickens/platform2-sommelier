// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "biod/ec_command.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace biod {
namespace {

constexpr int kDummyFd = 0;
constexpr int kIoctlFailureRetVal = -1;

template <typename O, typename I>
class MockEcCommand : public EcCommand<O, I> {
 public:
  using EcCommand<O, I>::EcCommand;
  ~MockEcCommand() override = default;

  using Data = typename EcCommand<O, I>::Data;
  MOCK_METHOD(int, ioctl, (int fd, uint32_t request, Data* data));
};

class MockFpModeCommand : public MockEcCommand<struct ec_params_fp_mode,
                                               struct ec_response_fp_mode> {
 public:
  MockFpModeCommand() : MockEcCommand(EC_CMD_FP_MODE, 0, {.mode = 1}) {}
};

// ioctl behavior for EC commands:
//   returns sizeof(EC response) (>=0) on success, -1 on failure
//   cmd.result is error code from EC (EC_RES_SUCCESS, etc)

TEST(EcCommand, RunOnceSuccess) {
  MockFpModeCommand mock;
  EXPECT_CALL(mock, ioctl).WillOnce(Return(mock.RespSize()));
  EXPECT_TRUE(mock.Run(kDummyFd));
}

TEST(EcCommand, RunOnceFailure) {
  MockFpModeCommand mock;
  EXPECT_CALL(mock, ioctl).WillOnce(Return(kIoctlFailureRetVal));
  EXPECT_FALSE(mock.Run(kDummyFd));
}

TEST(EcCommand, RunWithResultSuccess) {
  constexpr int kExpectedResult = 42;
  MockFpModeCommand mock;
  EXPECT_CALL(mock, ioctl)
      .WillOnce([](int, uint32_t, MockFpModeCommand::Data* data) {
        data->cmd.result = kExpectedResult;
        return data->cmd.insize;
      });
  uint16_t result = 0;
  EXPECT_TRUE(mock.Run(kDummyFd, 1, &result));
  EXPECT_EQ(result, kExpectedResult);
}

TEST(EcCommand, RunWithResultFailure) {
  MockFpModeCommand mock;
  EXPECT_CALL(mock, ioctl).WillOnce(Return(kIoctlFailureRetVal));
  uint16_t result = 0;
  EXPECT_FALSE(mock.Run(kDummyFd, 1, &result));
  EXPECT_EQ(result, 0xFF);
}

TEST(EcCommand, RunMultipleTimesSuccess) {
  constexpr int kNumAttempts = 2;
  MockFpModeCommand mock;
  EXPECT_CALL(mock, ioctl)
      .Times(kNumAttempts)
      // First ioctl() fails
      .WillOnce(InvokeWithoutArgs([]() {
        errno = ETIMEDOUT;
        return kIoctlFailureRetVal;
      }))
      // Second ioctl() succeeds
      .WillOnce(Return(mock.RespSize()));
  EXPECT_TRUE(mock.Run(kDummyFd, kNumAttempts));
}

TEST(EcCommand, RunMultipleTimesFailure) {
  constexpr int kNumAttempts = 2;
  MockFpModeCommand mock;
  EXPECT_CALL(mock, ioctl)
      .Times(kNumAttempts)
      // All calls to ioctl() timeout
      .WillRepeatedly(InvokeWithoutArgs([]() {
        errno = ETIMEDOUT;
        return kIoctlFailureRetVal;
      }));
  EXPECT_FALSE(mock.Run(kDummyFd, kNumAttempts));
}

}  // namespace
}  // namespace biod
