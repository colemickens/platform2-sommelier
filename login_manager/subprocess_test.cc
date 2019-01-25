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
using ::testing::SetArgPointee;

TEST(SubprocessTest, ForkAndKill) {
  const pid_t kDummyPid = 4;
  const gid_t kDummyGid = getgid();
  MockSystemUtils utils;
  auto subp = std::make_unique<login_manager::Subprocess>(getuid(), &utils);

  EXPECT_CALL(utils, GetGidAndGroups(getuid(), _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kDummyGid), Return(true)));
  EXPECT_CALL(utils, RunInMinijail(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(kDummyPid), Return(true)));
  ASSERT_TRUE(subp->ForkAndExec(std::vector<std::string>{"/bin/false"},
                                std::vector<std::string>()));

  EXPECT_CALL(utils, kill(kDummyPid, getuid(), SIGUSR1)).WillOnce(Return(0));
  subp->Kill(SIGUSR1);
}

}  // namespace login_manager
