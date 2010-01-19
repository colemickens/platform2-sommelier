// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <errno.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

#include "login_manager/bindings/client.h"
#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/system_utils.h"

namespace login_manager {

using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

class SessionManagerTest : public ::testing::Test { };

TEST(SessionManagerTest, NoLoopTest) {
  MockChildJob *job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .Times(1)
      .WillRepeatedly(Return(false));

  manager.Run();
}

static void BadExit() { exit(1); }  // compatible with void Run()
TEST(SessionManagerTest, BadExitChild) {
  MockChildJob *job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .Times(2)
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(BadExit));

  manager.Run();
}

static void CleanExit() { exit(0); }
TEST(SessionManagerTest, CleanExitChild) {
  MockChildJob* job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .Times(2)
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  manager.Run();
}

TEST(SessionManagerTest, MultiRunTest) {
  MockChildJob* job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .Times(3)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  manager.Run();
}

static const pid_t kDummyPid = 4;
TEST(SessionManagerTest, EasyCleanupTest) {
  MockChildJob* job = new MockChildJob;
  MockSystemUtils* utils = new MockSystemUtils;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);
  manager.set_child_pgid(kDummyPid);
  manager.set_systemutils(utils);

  int tries = 3;
  EXPECT_CALL(*utils, child_is_gone(kDummyPid))
      .Times(2)
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*utils, kill(kDummyPid, SIGTERM))
      .Times(1)
      .WillOnce(Return(0));

  manager.CleanupChildren(tries);
}

TEST(SessionManagerTest, HarderCleanupTest) {
  MockChildJob* job = new MockChildJob;
  MockSystemUtils* utils = new MockSystemUtils;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);
  manager.set_child_pgid(kDummyPid);
  manager.set_systemutils(utils);

  int tries = 3;
  EXPECT_CALL(*utils, child_is_gone(kDummyPid))
      .Times(tries)
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*utils, kill(kDummyPid, SIGTERM))
      .Times(tries - 1)
      .WillRepeatedly(Return(0));

  manager.CleanupChildren(tries);
}

TEST(SessionManagerTest, KillCleanupTest) {
  MockChildJob* job = new MockChildJob;
  MockSystemUtils* utils = new MockSystemUtils;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);
  manager.set_child_pgid(kDummyPid);
  manager.set_systemutils(utils);

  int tries = 3;
  EXPECT_CALL(*utils, child_is_gone(kDummyPid))
      .Times(tries + 2)
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  {
    InSequence dummy;
    EXPECT_CALL(*utils, kill(kDummyPid, SIGTERM))
        .Times(tries)
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*utils, kill(kDummyPid, SIGKILL))
        .Times(1)
        .WillOnce(Return(0));
  }

  manager.CleanupChildren(tries);
}

TEST(SessionManagerTest, StartSessionTest) {
  MockChildJob* job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*job, Toggle())
      .Times(1);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  manager.StartSession(email, nothing, &out, NULL);
}

TEST(SessionManagerTest, StopSessionTest) {
  MockChildJob* job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*job, Toggle())
      .Times(1);

  gboolean out;
  gchar nothing[] = "";
  manager.StopSession(nothing, &out, NULL);
}

TEST(SessionManagerTest, EmailAddressTest) {
  const char valid[] = "user@somewhere";
  EXPECT_TRUE(login_manager::SessionManagerService::ValidateEmail(valid));
}

TEST(SessionManagerTest, EmailAddressNonAsciiTest) {
  char invalid[4] = "a@m";
  invalid[2] = 254;
  EXPECT_FALSE(login_manager::SessionManagerService::ValidateEmail(invalid));
}

TEST(SessionManagerTest, EmailAddressNoAtTest) {
  const char no_at[] = "user";
  EXPECT_FALSE(login_manager::SessionManagerService::ValidateEmail(no_at));
}

TEST(SessionManagerTest, EmailAddressTooMuchAtTest) {
  const char extra_at[] = "user@what@where";
  EXPECT_FALSE(login_manager::SessionManagerService::ValidateEmail(extra_at));
}


}  // namespace login_manager
