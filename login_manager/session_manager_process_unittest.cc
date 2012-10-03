// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_unittest.h"

#include <errno.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/scoped_temp_dir.h>
#include <base/string_util.h>

#include "login_manager/child_job.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_child_process.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_system_utils.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

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
  static const int kDummyPid2;
  static const int kExit;

  enum RestartPolicy {
    ALWAYS, NEVER
  };

  void ExpectChildJobBoilerplate(MockChildJob* job, int clear_count) {
    EXPECT_CALL(*job, ClearOneTimeArgument()).Times(clear_count);
    EXPECT_CALL(*job, AddOneTimeArgument(
        SessionManagerService::kFirstBootFlag)).Times(1);
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
    InitManager(job, NULL);
    SetFileCheckerPolicy(child_runs);
    return job;
  }

  // Creates one job and a manager for it, running it |child_runs| times.
  void InitManagerWithRestartPolicy(RestartPolicy child_runs) {
    InitManager(new MockChildJob(), NULL);
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
const int SessionManagerProcessTest::kDummyPid2 = kDummyPid + 1;

TEST_F(SessionManagerProcessTest, NoLoopTest) {
  InitManagerWithRestartPolicy(NEVER);
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, BadExitChildFlagFileStop) {
  MockChildJob* job = new MockChildJob();
  EXPECT_CALL(*job, RecordTime()).Times(1);
  EXPECT_CALL(*job, ShouldStop()).Times(1).WillOnce(Return(false));
  ExpectChildJobBoilerplate(job, 1);
  InitManager(job, NULL);

  EXPECT_CALL(*file_checker_, exists())
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  MockChildProcess proc(kDummyPid, PackStatus(kExit), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .Times(AnyNumber())
      .WillRepeatedly(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                            Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, BadExitChildOnSignal) {
  MockChildJob* job = new MockChildJob();
  EXPECT_CALL(*job, RecordTime()).Times(1);
  EXPECT_CALL(*job, ShouldStop()).Times(1).WillOnce(Return(true));
  ExpectChildJobBoilerplate(job, 1);
  InitManager(job, NULL);
  SetFileCheckerPolicy(ALWAYS);

  MockChildProcess proc(kDummyPid, PackSignal(SIGILL), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .Times(AnyNumber())
      .WillRepeatedly(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                            Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, BadExitChild1) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  ExpectChildJobBoilerplate(job1, 2);
  ExpectChildJobBoilerplate(job2, 1);
  InitManager(job1, job2);

  SetFileCheckerPolicy(ALWAYS);
  EXPECT_CALL(*job1, RecordTime())
      .Times(2);
  EXPECT_CALL(*job2, RecordTime())
      .Times(1);
  EXPECT_CALL(*job1, ShouldStop())
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  MockChildProcess proc(kDummyPid, PackStatus(kExit), manager_->test_api());
  MockChildProcess proc2(kDummyPid2, PackStatus(kExit), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())))
      .WillOnce(Return(proc2.pid()))
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, BadExitChild2) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  ExpectChildJobBoilerplate(job1, 1);
  ExpectChildJobBoilerplate(job2, 2);
  InitManager(job1, job2);

  SetFileCheckerPolicy(ALWAYS);
  EXPECT_CALL(*job1, RecordTime())
      .Times(1);
  EXPECT_CALL(*job2, RecordTime())
      .Times(2);
  EXPECT_CALL(*job2, ShouldStop())
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  MockChildProcess proc(kDummyPid, PackStatus(kExit), manager_->test_api());
  MockChildProcess proc2(kDummyPid2, PackStatus(kExit), manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(Return(proc.pid()))
      .WillOnce(DoAll(Invoke(&proc2, &MockChildProcess::ScheduleExit),
                      Return(proc2.pid())))
      .WillOnce(DoAll(Invoke(&proc2, &MockChildProcess::ScheduleExit),
                      Return(proc2.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, CleanExitChild) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));
  ExpectChildJobBoilerplate(job, 1);

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, CleanExitChild2) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  ExpectChildJobBoilerplate(job1, 1);
  ExpectChildJobBoilerplate(job2, 1);
  InitManager(job1, job2);
  // Let the manager cause the clean exit.
  manager_->test_api().set_exit_on_child_done(false);

  SetFileCheckerPolicy(ALWAYS);
  EXPECT_CALL(*job1, RecordTime())
      .Times(1);
  EXPECT_CALL(*job2, RecordTime())
      .Times(1);
  EXPECT_CALL(*job2, ShouldStop())
      .WillOnce(Return(true));

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  MockChildProcess proc2(kDummyPid2, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(Return(proc.pid()))
      .WillOnce(DoAll(Invoke(&proc2, &MockChildProcess::ScheduleExit),
                      Return(proc2.pid())));

  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, LockedExit) {
  MockChildJob* job1 = new MockChildJob;
  MockChildJob* job2 = new MockChildJob;
  ExpectChildJobBoilerplate(job1, 1);
  ExpectChildJobBoilerplate(job2, 1);
  InitManager(job1, job2);
  // Let the manager cause the clean exit.
  manager_->test_api().set_exit_on_child_done(false);

  SetFileCheckerPolicy(ALWAYS);

  EXPECT_CALL(*job1, RecordTime())
      .Times(1);
  EXPECT_CALL(*job2, RecordTime())
      .Times(1);
  EXPECT_CALL(*job1, ShouldStop())
      .Times(0);

  manager_->test_api().set_screen_locked(true);

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  MockChildProcess proc2(kDummyPid2, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(Return(proc.pid()))
      .WillOnce(DoAll(Invoke(&proc2, &MockChildProcess::ScheduleExit),
                      Return(proc2.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, MustStopChild) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  ExpectChildJobBoilerplate(job, 1);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));
  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));
  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, KeygenExitTest) {
  MockChildJob* normal_job = new MockChildJob;
  InitManager(normal_job, NULL);
  manager_->test_api().set_child_pid(0, kDummyPid);

  ScopedTempDir tmpdir;
  FilePath key_file_path;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir.path(),
                                                  &key_file_path));
  std::string key_file_name(key_file_path.value());

  MockKeyGenerator* key_gen = new MockKeyGenerator;
  manager_->test_api().set_keygen(key_gen);
  EXPECT_CALL(*key_gen, temporary_key_filename())
      .WillOnce(ReturnRef(key_file_name));
  EXPECT_CALL(*device_policy_service_, ValidateAndStoreOwnerKey(_, _))
      .WillOnce(Return(true));

  SessionManagerService::HandleKeygenExit(kDummyPid,
                                          PackStatus(0),
                                          manager_.get());
  EXPECT_FALSE(file_util::PathExists(key_file_path));
}

TEST_F(SessionManagerProcessTest, StatsRecorded) {
  MockChildJob* job = CreateMockJobWithRestartPolicy(ALWAYS);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(DoAll(Invoke(&proc, &MockChildProcess::ScheduleExit),
                      Return(proc.pid())));

  EXPECT_CALL(*metrics_, RecordStats("chrome-exec")).Times(1);

  SimpleRunManager();
  DLOG(INFO) << "Finished the run!";
}

TEST_F(SessionManagerProcessTest, EnableChromeTesting) {
  MockChildJob* job = new MockChildJob;
  InitManager(job, NULL);
  EXPECT_CALL(*job, GetName()).WillRepeatedly(Return("chrome"));
  EXPECT_CALL(*job, SetExtraArguments(_)).Times(1);
  EXPECT_CALL(*job, RecordTime()).Times(AnyNumber());
  MockUtils();

  // DBus arrays are null-terminated.
  const char* args1[] = {"--repeat-arg", "--one-time-arg", NULL};
  const char* args2[] = {"--dummy", "--repeat-arg", NULL};
  gchar* testing_path = NULL;
  gchar* file_path = NULL;

  // Initial config...one running process that'll get SIGKILL'd.
  MockChildProcess proc(kDummyPid, -SIGKILL, manager_->test_api());
  EXPECT_CALL(utils_, kill(-proc.pid(), getuid(), SIGKILL)).WillOnce(Return(0));
  manager_->test_api().set_child_pid(0, proc.pid());

  // Expect a new chrome process to get spawned.
  MockChildProcess proc2(kDummyPid + 1, -SIGKILL, manager_->test_api());
  EXPECT_CALL(utils_, fork()).WillOnce(Return(proc2.pid()));
  EXPECT_TRUE(manager_->EnableChromeTesting(false, args1, &testing_path, NULL));
  ASSERT_TRUE(testing_path != NULL);

  // Now that we have the testing channel we can predict the
  // arguments that will be passed to SetExtraArguments().
  // We should see the same testing channel path in the subsequent invocation.
  std::string testing_argument = "--testing-channel=NamedTestingInterface:";
  testing_argument.append(testing_path);
  std::vector<std::string> extra_arguments(args2, args2 + arraysize(args2) - 1);
  extra_arguments.push_back(testing_argument);
  EXPECT_CALL(*job, SetExtraArguments(extra_arguments))
      .Times(1);
  EXPECT_CALL(utils_, kill(-proc2.pid(), getuid(), SIGKILL))
      .WillOnce(Return(0));

  // This invocation should do everything again, since force_relaunch is true.
  // Expect a new chrome process to get spawned.
  MockChildProcess proc3(kDummyPid + 2, -SIGKILL, manager_->test_api());
  EXPECT_CALL(utils_, fork()).WillOnce(Return(proc3.pid()));
  EXPECT_TRUE(manager_->EnableChromeTesting(true, args2, &file_path, NULL));
  ASSERT_TRUE(file_path != NULL);
  EXPECT_EQ(std::string(testing_path), std::string(file_path));

  // This invocation should do nothing.
  EXPECT_TRUE(manager_->EnableChromeTesting(false, args2, &file_path, NULL));
  ASSERT_TRUE(file_path != NULL);
  EXPECT_EQ(std::string(testing_path), std::string(file_path));
}

}  // namespace login_manager
