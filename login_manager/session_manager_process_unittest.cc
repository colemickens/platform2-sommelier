// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_unittest.h"

#include <errno.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/message_loop_proxy.h>
#include <base/memory/ref_counted.h>
#include <base/scoped_temp_dir.h>
#include <base/string_util.h>

#include "login_manager/child_job.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_child_process.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_liveness_checker.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_session_manager.h"
#include "login_manager/mock_system_utils.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::WithArgs;
using ::testing::_;

using std::string;

namespace login_manager {

// Used as a fixture for the tests in this file.
// Gives useful shared functionality.
class SessionManagerProcessTest : public SessionManagerTest {
 public:
  SessionManagerProcessTest() {}

  virtual ~SessionManagerProcessTest() {}

 protected:
  static const char kUptimeFile[];
  static const char kDiskFile[];
  static const int kExit;

  enum RestartPolicy {
    ALWAYS, NEVER
  };

  void ExpectLivenessChecking() {
    EXPECT_CALL(*liveness_checker_, Start()).Times(AtLeast(1));
    EXPECT_CALL(*liveness_checker_, Stop()).Times(AtLeast(1));
  }

  void ExpectOneTimeArgBoilerplate(MockChildJob* job) {
    EXPECT_CALL(*job, ClearOneTimeArgument()).Times(AtLeast(1));
    EXPECT_CALL(*metrics_, HasRecordedChromeExec())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*metrics_, RecordStats(StrEq(("chrome-exec"))))
        .Times(AnyNumber());
  }

  void ExpectChildJobBoilerplate(MockChildJob* job) {
    ExpectOneTimeArgBoilerplate(job);
    EXPECT_CALL(*job, RecordTime()).Times(1);
    ExpectLivenessChecking();
  }

  // Configures |file_checker_| to allow child restarting according to
  // |child_runs|.
  void SetFileCheckerPolicy(RestartPolicy child_runs) {
    switch (child_runs) {
    case ALWAYS:
      EXPECT_CALL(*file_checker_, exists())
          .WillRepeatedly(Return(false))
          .RetiresOnSaturation();
      break;
    case NEVER:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(true))
          .RetiresOnSaturation();
      break;
    default:
      NOTREACHED();
    }
  }

  // Creates one job and a manager for it, running it |child_runs| times.
  // Returns the job for further mocking.
  MockChildJob* CreateMockJobWithRestartPolicy(RestartPolicy child_runs) {
    MockChildJob* job = new MockChildJob();
    InitManager(job);
    SetFileCheckerPolicy(child_runs);
    return job;
  }

  // Creates one job and a manager for it, running it |child_runs| times.
  void InitManagerWithRestartPolicy(RestartPolicy child_runs) {
    InitManager(new MockChildJob());
    SetFileCheckerPolicy(child_runs);
  }

  int PackStatus(int status) { return __W_EXITCODE(status, 0); }
  int PackSignal(int signal) { return __W_EXITCODE(0, signal); }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerProcessTest);
};

// static
const char SessionManagerProcessTest::kUptimeFile[] = "/tmp/uptime-chrome-exec";
const char SessionManagerProcessTest::kDiskFile[] = "/tmp/disk-chrome-exec";
const int SessionManagerProcessTest::kExit = 1;

TEST_F(SessionManagerProcessTest, CleanupChildren) {
  InitManager(new MockChildJob);
  manager_->test_api().set_browser_pid(kDummyPid);

  int timeout = 3;
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));
  MockUtils();

  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerProcessTest, SlowKillCleanupChildren) {
  InitManager(new MockChildJob);
  manager_->test_api().set_browser_pid(kDummyPid);

  int timeout = 3;
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGABRT))
      .WillOnce(Return(0));
  MockUtils();

  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerProcessTest, SessionStartedCleanup) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);

  // Expect the job to be run.
  EXPECT_CALL(utils_, fork()).WillOnce(Return(kDummyPid));
  ExpectChildJobBoilerplate(job);

  ExpectSuccessfulInitialization();
  ExpectShutdown();

  // Expect the job to be killed, and die promptly.
  int timeout = 3;
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));

  MockUtils();

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SessionManagerService::Shutdown),
                 manager_.get()));
  manager_->Run();
}

TEST_F(SessionManagerProcessTest, SessionStartedSlowKillCleanup) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);

  // Expect the job to be run.
  EXPECT_CALL(utils_, fork()).WillOnce(Return(kDummyPid));
  ExpectChildJobBoilerplate(job);

  ExpectSuccessfulInitialization();
  ExpectShutdown();

  int timeout = 3;
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGABRT))
      .WillOnce(Return(0));

  MockUtils();

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SessionManagerService::Shutdown),
                 manager_.get()));
  manager_->Run();
}

TEST_F(SessionManagerProcessTest, BadExitChildFlagFileStop) {
  MockChildJob* job = new MockChildJob();
  ExpectChildJobBoilerplate(job);
  InitManager(job);

  EXPECT_CALL(*job, ShouldStop()).WillOnce(Return(false));
  EXPECT_CALL(*file_checker_, exists())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  MockChildProcess proc(kDummyPid, PackStatus(kExit), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .Times(AnyNumber())
      .WillRepeatedly(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                            Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, BadExitChildOnSignal) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectChildJobBoilerplate(job);

  EXPECT_CALL(*job, ShouldStop()).WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  MockChildProcess proc(kDummyPid, PackSignal(SIGILL), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .Times(AnyNumber())
      .WillRepeatedly(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                            Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, BadExitChild) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectLivenessChecking();
  ExpectOneTimeArgBoilerplate(job);

  EXPECT_CALL(*job, RecordTime()).Times(2);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  MockChildProcess proc(kDummyPid, PackStatus(kExit), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())))
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, CleanExitChild) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectChildJobBoilerplate(job);

  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, LockedExit) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectChildJobBoilerplate(job);
  // Let the manager cause the clean exit.
  manager_->test_api().set_exit_on_child_done(false);

  EXPECT_CALL(*job, ShouldStop())
      .Times(0);

  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillOnce(Return(true));

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, FirstBootFlagUsedOnce) {
  // job should run, die, and get run again.  On its first run, it should
  // have a one-time-flag.  That should get cleared and not used again.
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);

  EXPECT_CALL(*metrics_, HasRecordedChromeExec())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*metrics_, RecordStats(StrEq(("chrome-exec"))))
      .Times(2);
  EXPECT_CALL(*job, AddOneTimeArgument(
      StrEq(SessionManagerService::kFirstBootFlag))).Times(1);
  EXPECT_CALL(*job, ClearOneTimeArgument()).Times(2);

  ExpectLivenessChecking();
  EXPECT_CALL(*job, RecordTime()).Times(2);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  MockChildProcess proc(kDummyPid, PackStatus(kExit), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())))
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, LivenessCheckingStartStop) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectOneTimeArgBoilerplate(job);
  {
    Sequence start_stop;
    EXPECT_CALL(*liveness_checker_, Start()).Times(2);
    EXPECT_CALL(*liveness_checker_, Stop()).Times(AtLeast(2));
  }
  EXPECT_CALL(*job, RecordTime()).Times(2);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  MockChildProcess proc(kDummyPid, PackStatus(kExit), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())))
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, MustStopChild) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectChildJobBoilerplate(job);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));
  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, KeygenExitTest) {
  MockChildJob* normal_job = new MockChildJob;
  InitManager(normal_job);

  FilePath key_file_path("some/where/fake");
  string key_file_name(key_file_path.value());

  MockKeyGenerator* key_gen = new MockKeyGenerator;
  manager_->test_api().set_keygen(key_gen);
  EXPECT_CALL(*key_gen, temporary_key_filename())
      .WillOnce(ReturnRef(key_file_name));
  EXPECT_CALL(*session_manager_impl_,
              ImportValidateAndStoreGeneratedKey(key_file_path))
      .Times(1);

  SessionManagerService::HandleKeygenExit(kDummyPid,
                                          PackStatus(0),
                                          manager_.get());
}

TEST_F(SessionManagerProcessTest, StatsRecorded) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectChildJobBoilerplate(job);
  // Override looser expectation from ExpectChildJobBoilerplate().
  EXPECT_CALL(*metrics_, RecordStats("chrome-exec")).Times(1);

  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));

  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, TestWipeOnBadState) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);

  // Expected to occur during manager_->Run().
  ExpectChildJobBoilerplate(job);
  EXPECT_CALL(*session_manager_impl_, Initialize())
    .WillOnce(Return(false));
  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));

  // Expect Powerwash to be triggered.
  EXPECT_CALL(*session_manager_impl_, StartDeviceWipe(_, _))
    .WillOnce(Return(true));
  MockUtils();

  ASSERT_FALSE(manager_->Run());
}

}  // namespace login_manager
