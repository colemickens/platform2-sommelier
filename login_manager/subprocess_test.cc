// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/subprocess.h"

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/mock_system_utils.h"

namespace login_manager {

using ::testing::_;
using ::testing::Return;

TEST(SubprocessTest, ForkAndKill) {
  const pid_t kDummyPid = 4;
  MockSystemUtils utils;
  auto subp = std::make_unique<login_manager::Subprocess>(getuid(), &utils);
  EXPECT_CALL(utils, GetGidAndGroups(getuid(), _, _)).WillOnce(Return(true));
  EXPECT_CALL(utils, fork()).WillOnce(Return(kDummyPid));
  ASSERT_TRUE(subp->ForkAndExec(std::vector<std::string>{"/bin/false"},
                                std::vector<std::string>()));
  EXPECT_CALL(utils, kill(kDummyPid, getuid(), SIGUSR1)).WillOnce(Return(0));
  subp->Kill(SIGUSR1);
}

TEST(SubprocessTest, ForkAndExec) {
  MockSystemUtils utils;
  auto subp = std::make_unique<login_manager::Subprocess>(getuid(), &utils);
  EXPECT_CALL(utils, GetGidAndGroups(getuid(), _, _)).WillOnce(Return(true));
  EXPECT_CALL(utils, SetIDs(getuid(), _, _)).WillOnce(Return(0));
  EXPECT_CALL(utils, CloseSuperfluousFds(_));
  // Pretend we're in the child process.
  EXPECT_CALL(utils, fork()).WillOnce(Return(0));
  // We need to make execve() return for the test to actually finish.
  EXPECT_CALL(utils, execve(_, _, _)).WillOnce(Return(0));
  ASSERT_FALSE(subp->ForkAndExec(std::vector<std::string>{"/bin/false"},
                                 std::vector<std::string>()));
}

TEST(SubprocessTest, ForkAndExecInNewMountNamespace) {
  MockSystemUtils utils;
  auto subp = std::make_unique<login_manager::Subprocess>(getuid(), &utils);
  subp->UseNewMountNamespace();
  EXPECT_CALL(utils, GetGidAndGroups(getuid(), _, _)).WillOnce(Return(true));
  EXPECT_CALL(utils, EnterNewMountNamespace()).WillOnce(Return(true));
  EXPECT_CALL(utils, SetIDs(getuid(), _, _)).WillOnce(Return(0));
  EXPECT_CALL(utils, CloseSuperfluousFds(_));
  // Pretend we're in the child process.
  EXPECT_CALL(utils, fork()).WillOnce(Return(0));
  EXPECT_CALL(utils, execve(_, _, _)).WillOnce(Return(0));
  ASSERT_FALSE(subp->ForkAndExec(std::vector<std::string>{"/bin/false"},
                                 std::vector<std::string>()));
}

}  // namespace login_manager
