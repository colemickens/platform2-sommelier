// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <errno.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "login_manager/bindings/client.h"
#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/system_utils.h"

namespace login_manager {

using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;
using ::testing::AnyNumber;

static const char kCheckedFile[] = "/tmp/checked_file";
static const char kUptimeFile1[] = "/tmp/uptime-job1-exec";
static const char kDiskFile1[] = "/tmp/disk-job1-exec";
static const char kUptimeFile2[] = "/tmp/uptime-job2-exec";
static const char kDiskFile2[] = "/tmp/disk-job2-exec";

// Used as a base class for the tests in this file.
// Gives useful shared functionality.
class SessionManagerTest : public ::testing::Test {
 public:
  SessionManagerTest() {
    manager_ = NULL;
    utils_ = new MockSystemUtils;
    file_checker_ = new MockFileChecker(kCheckedFile);
  }

  virtual ~SessionManagerTest() {
    delete manager_;
    FilePath uptime1(kUptimeFile1);
    FilePath disk1(kDiskFile1);
    FilePath uptime2(kUptimeFile2);
    FilePath disk2(kDiskFile2);
    file_util::Delete(uptime1, false);
    file_util::Delete(disk1, false);
    file_util::Delete(uptime2, false);
    file_util::Delete(disk2, false);
  }

 protected:
  // Creates the manager with the jobs. Mocks the file checker.
  // The second job can be NULL.
  void InitManager(MockChildJob* job1, MockChildJob* job2) {
    std::vector<ChildJobInterface*> jobs;
    EXPECT_CALL(*job1, GetName())
        .WillRepeatedly(Return(std::string("job1")));
    jobs.push_back(job1);
    if (job2) {
      EXPECT_CALL(*job2, GetName())
          .WillRepeatedly(Return(std::string("job2")));
      jobs.push_back(job2);
    }
    manager_ = new SessionManagerService(jobs);
    manager_->set_file_checker(file_checker_);
    manager_->test_api().set_exit_on_child_done(true);
  }

  void Run() {
    manager_->Run();
  }

  void MockUtils() {
    manager_->test_api().set_systemutils(utils_);
  }

  enum ChildRuns {
    ALWAYS, NEVER, ONCE, EXACTLY_ONCE, TWICE, MAYBE_NEVER
  };

  // Configures the file_checker_ to run the children.
  void RunChildren(ChildRuns child_runs) {
    switch (child_runs) {
    case ALWAYS:
      EXPECT_CALL(*file_checker_, exists())
          .WillRepeatedly(Return(false));
      break;
    case NEVER:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(true));
      break;
    case ONCE:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(false))
          .WillOnce(Return(true));
      break;
    case EXACTLY_ONCE:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(false));
      break;
    case TWICE:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(false))
          .WillOnce(Return(false))
          .WillOnce(Return(true));
      break;
    case MAYBE_NEVER:
      // If it's called, don't run.
      EXPECT_CALL(*file_checker_, exists())
          .Times(AnyNumber())
          .WillOnce(Return(true));
    }
  }

  // Creates one job and a manager for it, running it |child_runs| times.
  MockChildJob* CreateTrivialMockJob(ChildRuns child_runs) {
    MockChildJob* job = new MockChildJob();
    InitManager(job, NULL);
    RunChildren(child_runs);

    return job;
  }

  SessionManagerService* manager_;
  MockSystemUtils* utils_;
  MockFileChecker* file_checker_;
};

// compatible with void Run()
static void BadExit() { _exit(1); }
static void BadExitAfterSleep() { sleep(1); _exit(1); }
static void RunAndSleep() { while (true) { sleep(1); } };
static void CleanExit() { _exit(0); }

TEST_F(SessionManagerTest, NoLoopTest) {
  MockChildJob* job = CreateTrivialMockJob(NEVER);
  Run();
}

TEST_F(SessionManagerTest, BadExitChild) {
  MockChildJob* job = CreateTrivialMockJob(ONCE);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .Times(1)
      .WillOnce(Return(false));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(BadExit));

  Run();
}

TEST_F(SessionManagerTest, BadExitChild1) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  InitManager(job1, job2);

  RunChildren(TWICE);
  EXPECT_CALL(*job1, RecordTime())
      .Times(2);
  EXPECT_CALL(*job2, RecordTime())
      .Times(1);
  EXPECT_CALL(*job1, ShouldStop())
      .Times(2);
  ON_CALL(*job1, Run())
      .WillByDefault(Invoke(BadExitAfterSleep));
  ON_CALL(*job2, Run())
      .WillByDefault(Invoke(RunAndSleep));

  Run();
}

TEST_F(SessionManagerTest, BadExitChild2) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  InitManager(job1, job2);

  RunChildren(TWICE);
  EXPECT_CALL(*job1, RecordTime())
      .Times(1);
  EXPECT_CALL(*job2, RecordTime())
      .Times(2);
  EXPECT_CALL(*job2, ShouldStop())
      .Times(2);
  ON_CALL(*job1, Run())
      .WillByDefault(Invoke(RunAndSleep));
  ON_CALL(*job2, Run())
      .WillByDefault(Invoke(BadExitAfterSleep));

  Run();
}

TEST_F(SessionManagerTest, CleanExitChild) {
  MockChildJob* job = CreateTrivialMockJob(EXACTLY_ONCE);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  Run();
}

TEST_F(SessionManagerTest, CleanExitChild2) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  InitManager(job1, job2);
  // Let the manager cause the clean exit.
  manager_->test_api().set_exit_on_child_done(false);

  RunChildren(EXACTLY_ONCE);
  EXPECT_CALL(*job1, RecordTime())
      .Times(1);
  EXPECT_CALL(*job2, RecordTime())
      .Times(1);
  ON_CALL(*job1, Run())
      .WillByDefault(Invoke(RunAndSleep));
  ON_CALL(*job2, Run())
      .WillByDefault(Invoke(CleanExit));

  Run();
}

TEST_F(SessionManagerTest, LockedExit) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  InitManager(job1, job2);
  // Let the manager cause the clean exit.
  manager_->test_api().set_exit_on_child_done(false);

  RunChildren(ALWAYS);

  EXPECT_CALL(*job1, RecordTime())
      .Times(1);
  EXPECT_CALL(*job2, RecordTime())
      .Times(1);
  EXPECT_CALL(*job1, ShouldStop())
      .Times(0);

  manager_->test_api().set_screen_locked(true);
  ON_CALL(*job1, Run())
      .WillByDefault(Invoke(BadExitAfterSleep));
  ON_CALL(*job2, Run())
      .WillByDefault(Invoke(RunAndSleep));

  Run();
}

TEST_F(SessionManagerTest, MustStopChild) {
  MockChildJob* job = CreateTrivialMockJob(ALWAYS);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .Times(1)
      .WillOnce(Return(true));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(BadExit));

  Run();
}

static const pid_t kDummyPid = 4;
TEST_F(SessionManagerTest, SessionNotStartedCleanupTest) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  manager_->test_api().set_child_pid(0, kDummyPid);

  int timeout = 3;
  EXPECT_CALL(*utils_, kill(kDummyPid, SIGKILL))
      .WillOnce(Return(0));
  EXPECT_CALL(*utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));

  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerTest, SessionNotStartedSlowKillCleanupTest) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  manager_->test_api().set_child_pid(0, kDummyPid);

  int timeout = 3;
  EXPECT_CALL(*utils_, kill(kDummyPid, SIGKILL))
      .WillOnce(Return(0));
  EXPECT_CALL(*utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(*utils_, kill(kDummyPid, SIGABRT))
      .WillOnce(Return(0));

  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerTest, SessionStartedCleanupTest) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  int timeout = 3;
  EXPECT_CALL(*utils_, kill(kDummyPid, SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(*utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));

  std::string email_string(email);
  EXPECT_CALL(*job, StartSession(email_string))
      .Times(1);

  manager_->StartSession(email, nothing, &out, NULL);
  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerTest, SessionStartedSlowKillCleanupTest) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  SessionManagerService::TestApi test_api = manager_->test_api();
  test_api.set_child_pid(0, kDummyPid);

  int timeout = 3;
  EXPECT_CALL(*utils_, kill(kDummyPid, SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(*utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(*utils_, kill(kDummyPid, SIGABRT))
      .WillOnce(Return(0));

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";

  std::string email_string(email);
  EXPECT_CALL(*job, StartSession(email_string))
      .Times(1);

  manager_->StartSession(email, nothing, &out, NULL);
  test_api.CleanupChildren(timeout);
}

static void Sleep() { execl("/bin/sleep", "sleep", "10000", NULL); }
TEST_F(SessionManagerTest, SessionStartedSigTermTest) {
  int pid = fork();
  if (pid == 0) {
    MockChildJob* job = CreateTrivialMockJob(EXACTLY_ONCE);

    gboolean out;
    gchar email[] = "user@somewhere";
    gchar nothing[] = "";
    std::string email_string(email);

    EXPECT_CALL(*job, RecordTime())
        .Times(1);
    ON_CALL(*job, Run())
        .WillByDefault(Invoke(Sleep));
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);

    manager_->StartSession(email, nothing, &out, NULL);
    Run();
    delete manager_;
    manager_ = NULL;
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

TEST_F(SessionManagerTest, StartSessionTest) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  std::string email_string(email);
  EXPECT_CALL(*job, StartSession(email_string))
      .Times(1);
  manager_->StartSession(email, nothing, &out, NULL);
}

TEST_F(SessionManagerTest, StartSessionErrorTest) {
  MockChildJob* job = CreateTrivialMockJob(ALWAYS);
  gboolean out;
  gchar email[] = "user";
  gchar nothing[] = "";
  GError* error = NULL;
  manager_->StartSession(email, nothing, &out, &error);
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_INVALID_EMAIL, error->code);
  g_error_free(error);
}

TEST_F(SessionManagerTest, StopSessionTest) {
  MockChildJob* job = CreateTrivialMockJob(ALWAYS);
  gboolean out;
  gchar nothing[] = "";
  manager_->StopSession(nothing, &out, NULL);
}

static const std::string name() { return "foo"; }

TEST_F(SessionManagerTest, StatsRecorded) {
  FilePath uptime(kUptimeFile1);
  FilePath disk(kDiskFile1);
  file_util::Delete(uptime, false);
  file_util::Delete(disk, false);
  MockChildJob* job = CreateTrivialMockJob(EXACTLY_ONCE);
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  ON_CALL(*job, GetName())
      .WillByDefault(Invoke(name));
  Run();
  EXPECT_TRUE(file_util::PathExists(uptime));
  EXPECT_TRUE(file_util::PathExists(disk));
}

TEST_F(SessionManagerTest, RestartJobUnknownPid) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  SessionManagerService::TestApi test_api = manager_->test_api();
  test_api.set_child_pid(0, kDummyPid);

  gboolean out;
  gint pid = kDummyPid + 1;
  gchar arguments[] = "";
  GError* error = NULL;
  EXPECT_EQ(FALSE, manager_->RestartJob(pid, arguments, &out, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PID, error->code);
  EXPECT_EQ(FALSE, out);
}

TEST_F(SessionManagerTest, RestartJob) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  SessionManagerService::TestApi test_api = manager_->test_api();
  test_api.set_child_pid(0, kDummyPid);
  EXPECT_CALL(*utils_, kill(-kDummyPid, SIGKILL))
      .WillOnce(Return(0));

  EXPECT_CALL(*job, SetArguments("dummy"))
      .Times(1);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  gboolean out;
  gint pid = kDummyPid;
  gchar arguments[] = "dummy";
  GError* error = NULL;
  EXPECT_EQ(TRUE, manager_->RestartJob(pid, arguments, &out, &error));
  EXPECT_EQ(TRUE, out);
}

TEST(SessionManagerTestStatic, EmailAddressTest) {
  const char valid[] = "user@somewhere";
  EXPECT_TRUE(login_manager::SessionManagerService::ValidateEmail(valid));
}

TEST(SessionManagerTestStatic, EmailAddressNonAsciiTest) {
  char invalid[4] = "a@m";
  invalid[2] = 254;
  EXPECT_FALSE(login_manager::SessionManagerService::ValidateEmail(invalid));
}

TEST(SessionManagerTestStatic, EmailAddressNoAtTest) {
  const char no_at[] = "user";
  EXPECT_FALSE(login_manager::SessionManagerService::ValidateEmail(no_at));
}

TEST(SessionManagerTestStatic, EmailAddressTooMuchAtTest) {
  const char extra_at[] = "user@what@where";
  EXPECT_FALSE(login_manager::SessionManagerService::ValidateEmail(extra_at));
}

TEST(SessionManagerTestStatic, GetArgLists0) {
  std::vector<std::string> args;
  std::vector<std::vector<std::string> > arg_lists =
      SessionManagerService::GetArgLists(args);
  EXPECT_EQ(0, arg_lists.size());
}

static std::vector<std::vector<std::string> > GetArgs(const char** c_args) {
  std::vector<std::string> args;
  while (*c_args) {
    args.push_back(*c_args);
    c_args++;
  }
  return SessionManagerService::GetArgLists(args);
}

TEST(SessionManagerTestStatic, GetArgLists1) {
  const char* c_args[] = {"a", "b", "c", NULL};
  std::vector<std::vector<std::string> > arg_lists = GetArgs(c_args);
  EXPECT_EQ(1, arg_lists.size());
  EXPECT_EQ(3, arg_lists[0].size());
}

TEST(SessionManagerTestStatic, GetArgLists2) {
  const char* c_args[] = {"a", "b", "c", "--", "d", NULL};
  std::vector<std::vector<std::string> > arg_lists = GetArgs(c_args);
  EXPECT_EQ(2, arg_lists.size());
  EXPECT_EQ(3, arg_lists[0].size());
  EXPECT_EQ(1, arg_lists[1].size());
}

TEST(SessionManagerTestStatic, GetArgLists_TrailingDashes) {
  const char* c_args[] = {"a", "b", "c", "--", NULL};
  std::vector<std::vector<std::string> > arg_lists = GetArgs(c_args);
  EXPECT_EQ(1, arg_lists.size());
  EXPECT_EQ(3, arg_lists[0].size());
}

TEST(SessionManagerTestStatic, GetArgLists3_InitialDashes) {
  const char* c_args[] = {"--", "a", "b", "c", NULL};
  std::vector<std::vector<std::string> > arg_lists = GetArgs(c_args);
  EXPECT_EQ(1, arg_lists.size());
  EXPECT_EQ(3, arg_lists[0].size());
}

}  // namespace login_manager
