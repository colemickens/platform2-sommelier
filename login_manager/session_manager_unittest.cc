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

TEST(SessionManagerTest, MustStopChild) {
  MockChildJob *job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership

  EXPECT_CALL(*job, ShouldRun())
      .WillOnce(Return(true));
  EXPECT_CALL(*job, ShouldStop())
      .Times(1)
      .WillOnce(Return(true));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(BadExit));

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
TEST(SessionManagerTest, SessionNotStartedCleanupTest) {
  MockChildJob* job = new MockChildJob;
  MockSystemUtils* utils = new MockSystemUtils;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);
  manager.set_child_pid(kDummyPid);
  manager.set_systemutils(utils);

  int timeout = 3;
  EXPECT_CALL(*utils, kill(kDummyPid, SIGKILL))
      .WillOnce(Return(0));
  EXPECT_CALL(*utils, child_is_gone(kDummyPid, timeout))
      .WillOnce(Return(true));

  manager.CleanupChildren(timeout);
}

TEST(SessionManagerTest, SessionNotStartedSlowKillCleanupTest) {
  MockChildJob* job = new MockChildJob;
  MockSystemUtils* utils = new MockSystemUtils;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);
  manager.set_child_pid(kDummyPid);
  manager.set_systemutils(utils);

  int timeout = 3;
  EXPECT_CALL(*utils, kill(kDummyPid, SIGKILL))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*utils, child_is_gone(kDummyPid, timeout))
      .WillOnce(Return(false));

  manager.CleanupChildren(timeout);
}

MockChildJob* CreateTrivialMockJob() {
  MockChildJob* job = new MockChildJob;

  EXPECT_CALL(*job, ShouldRun())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*job, Toggle())
      .Times(1);

  return job;
}

TEST(SessionManagerTest, SessionStartedCleanupTest) {
  MockChildJob* job = CreateTrivialMockJob();
  MockSystemUtils* utils = new MockSystemUtils;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);
  manager.set_child_pid(kDummyPid);
  manager.set_systemutils(utils);

  int timeout = 3;
  EXPECT_CALL(*utils, kill(kDummyPid, SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(*utils, child_is_gone(kDummyPid, timeout))
      .WillOnce(Return(true));

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  manager.StartSession(email, nothing, &out, NULL);
  manager.CleanupChildren(timeout);
}

TEST(SessionManagerTest, SessionStartedSlowKillCleanupTest) {
  MockChildJob* job = CreateTrivialMockJob();
  MockSystemUtils* utils = new MockSystemUtils;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);
  manager.set_child_pid(kDummyPid);
  manager.set_systemutils(utils);

  int timeout = 3;
  EXPECT_CALL(*utils, kill(kDummyPid, SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(*utils, child_is_gone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(*utils, kill(kDummyPid, SIGKILL))
      .WillOnce(Return(0));

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  manager.StartSession(email, nothing, &out, NULL);
  manager.CleanupChildren(timeout);
}

static void Sleep() { execl("/bin/sleep", "sleep", "10000", NULL); }
TEST(SessionManagerTest, SessionStartedSigTermTest) {
  int pid = fork();
  if (pid == 0) {
    MockChildJob* job = CreateTrivialMockJob();
    login_manager::SessionManagerService* manager =
        new login_manager::SessionManagerService(job);

    ON_CALL(*job, Run())
        .WillByDefault(Invoke(Sleep));

    gboolean out;
    gchar email[] = "user@somewhere";
    gchar nothing[] = "";
    manager->StartSession(email, nothing, &out, NULL);

    manager->Run();
    delete manager;
    exit(0);
  }
  sleep(1);
  kill(pid, SIGTERM);
  int status;
  while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
    ;

  DLOG(INFO) << "exited waitpid. " << pid << "\n"
             << "  WIFSIGNALED is " << WIFSIGNALED(status) << "\n"
             << "  WTERMSIG is " << WTERMSIG(status) << "\n"
             << "  WIFEXITED is " << WIFEXITED(status) << "\n"
             << "  WEXITSTATUS is " << WEXITSTATUS(status);

  EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

TEST(SessionManagerTest, StartSessionTest) {
  MockChildJob* job = CreateTrivialMockJob();
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  manager.StartSession(email, nothing, &out, NULL);
}

TEST(SessionManagerTest, StartSessionErrorTest) {
  MockChildJob* job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .WillRepeatedly(Return(true));

  gboolean out;
  gchar email[] = "user";
  gchar nothing[] = "";
  GError* error = NULL;
  manager.StartSession(email, nothing, &out, &error);
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_INVALID_EMAIL, error->code);
  g_error_free(error);
}

TEST(SessionManagerTest, StopSessionTest) {
  MockChildJob* job = new MockChildJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  manager.set_exit_on_child_done(true);

  EXPECT_CALL(*job, ShouldRun())
      .WillRepeatedly(Return(true));

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
