// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <base/run_loop.h>
#include <base/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/browser_job.h"
#include "login_manager/fake_browser_job.h"
#include "login_manager/fake_child_process.h"
#include "login_manager/fake_generator_job.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_liveness_checker.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_session_manager.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/system_utils_impl.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::WithArgs;
using ::testing::_;

using std::string;

namespace login_manager {

// Used as a fixture for the tests in this file.
// Gives useful shared functionality.
class SessionManagerProcessTest : public ::testing::Test {
 public:
  SessionManagerProcessTest()
      : manager_(NULL),
        liveness_checker_(new MockLivenessChecker),
        session_manager_impl_(new MockSessionManager),
        must_destroy_mocks_(true) {
  }

  virtual ~SessionManagerProcessTest() {
    if (must_destroy_mocks_) {
      delete liveness_checker_;
      delete session_manager_impl_;
    }
  }

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
    must_destroy_mocks_ = !manager_.get();
    manager_ = NULL;
  }

 protected:
  // kFakeEmail is NOT const so that it can be passed to methods that
  // implement dbus calls, which (of necessity) take bare gchar*.
  static char kFakeEmail[];
  static const pid_t kDummyPid;
  static const int kExit;

  void MockUtils() {
    manager_->test_api().set_systemutils(&utils_);
  }

  void ExpectShutdown() {
    EXPECT_CALL(*session_manager_impl_, AnnounceSessionStoppingIfNeeded())
        .Times(1);
    EXPECT_CALL(*session_manager_impl_, AnnounceSessionStopped())
        .Times(1);
  }

  void ExpectLivenessChecking() {
    EXPECT_CALL(*liveness_checker_, Start()).Times(AtLeast(1));
    EXPECT_CALL(*liveness_checker_, Stop()).Times(AtLeast(1));
  }

  void ExpectOneJobReRun(FakeBrowserJob* job, int exit_status) {
    EXPECT_CALL(*job, KillEverything(SIGKILL, _)).Times(AnyNumber());
    EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*job, ShouldStop())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    job->set_fake_child_process(scoped_ptr<FakeChildProcess>(
        new FakeChildProcess(kDummyPid, exit_status, manager_->test_api())));
  }

  void InitManager(FakeBrowserJob* job) {
    manager_ = new SessionManagerService(scoped_ptr<BrowserJobInterface>(job),
                                         run_loop_.QuitClosure(),
                                         getuid(),
                                         3,
                                         false,
                                         base::TimeDelta(),
                                         &metrics_,
                                         &real_utils_);
    manager_->test_api().set_liveness_checker(liveness_checker_);
    manager_->test_api().set_session_manager(session_manager_impl_);
  }

  void SimpleRunManager() {
    ExpectShutdown();
    manager_->RunBrowser();
    run_loop_.Run();
  }

  void ForceRunLoop() {
    run_loop_.Run();
  }

  FakeBrowserJob* CreateMockJobAndInitManager(bool schedule_exit) {
    FakeBrowserJob* job = new FakeBrowserJob("FakeBrowserJob", schedule_exit);
    InitManager(job);

    job->set_fake_child_process(
        scoped_ptr<FakeChildProcess>(
            new FakeChildProcess(kDummyPid, 0, manager_->test_api())));

    return job;
  }

  int PackStatus(int status) { return __W_EXITCODE(status, 0); }
  int PackSignal(int signal) { return __W_EXITCODE(0, signal); }

  scoped_refptr<SessionManagerService> manager_;
  SystemUtilsImpl real_utils_;
  MockMetrics metrics_;
  MockSystemUtils utils_;

  // These are bare pointers, not scoped_ptrs, because we need to give them
  // to a SessionManagerService instance, but also be able to set expectations
  // on them after we hand them off.
  MockLivenessChecker* liveness_checker_;
  MockSessionManager* session_manager_impl_;

 private:
  bool must_destroy_mocks_;
  base::ScopedTempDir tmpdir_;
  MessageLoopForIO message_loop_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerProcessTest);
};

// static
char SessionManagerProcessTest::kFakeEmail[] = "cmasone@whaaat.org";
const pid_t SessionManagerProcessTest::kDummyPid = 4;
const int SessionManagerProcessTest::kExit = 1;

// Browser processes get correctly terminated.
TEST_F(SessionManagerProcessTest, CleanupBrowser) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(false);
  EXPECT_CALL(*job, Kill(SIGTERM, _)).Times(1);
  EXPECT_CALL(*job, WaitAndAbort(_)).Times(1);
  job->RunInBackground();
  manager_->test_api().CleanupChildren(3);
}

// Gracefully shut down while the browser is running.
TEST_F(SessionManagerProcessTest, BrowserRunningShutdown) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(false);

  ExpectLivenessChecking();
  ExpectShutdown();

  // Expect the job to be killed.
  EXPECT_CALL(*job, Kill(SIGTERM, _)).Times(1);
  EXPECT_CALL(*job, WaitAndAbort(_)).Times(1);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&SessionManagerService::RunBrowser,
                 manager_.get()));

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&SessionManagerService::ScheduleShutdown,
                 manager_.get()));

  ForceRunLoop();
}

// If the browser exits and asks to stop, the session manager
// should not restart it.
TEST_F(SessionManagerProcessTest, ChildExitFlagFileStop) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(true);
  manager_->test_api().set_exit_on_child_done(true);  // or it'll run forever.
  ExpectLivenessChecking();

  EXPECT_CALL(*job, KillEverything(SIGKILL, _)).Times(AnyNumber());
  EXPECT_CALL(*job, ShouldStop()).WillOnce(Return(false));
  job->set_should_run(false);

  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked()).WillOnce(Return(false));

  SimpleRunManager();
}

// A child that exits with a signal should get re-run.
TEST_F(SessionManagerProcessTest, BadExitChildOnSignal) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(true);
  ExpectLivenessChecking();
  ExpectOneJobReRun(job, PackSignal(SIGILL));
  SimpleRunManager();
}

// A child that exits badly should get re-run.
TEST_F(SessionManagerProcessTest, BadExitChild) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(true);
  ExpectLivenessChecking();
  ExpectOneJobReRun(job, PackSignal(kExit));
  SimpleRunManager();
}

// A child that exits cleanly should get re-run.
TEST_F(SessionManagerProcessTest, CleanExitChild) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(true);
  ExpectLivenessChecking();
  ExpectOneJobReRun(job, PackSignal(0));
  SimpleRunManager();
}

// If the browser exits while the screen is locked, the session manager
// should exit.
TEST_F(SessionManagerProcessTest, LockedExit) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(true);
  ExpectLivenessChecking();

  EXPECT_CALL(*job, KillEverything(SIGKILL, _)).Times(AnyNumber());
  EXPECT_CALL(*job, ShouldStop()).Times(0);

  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked()).WillOnce(Return(true));

  SimpleRunManager();
}

// Liveness checking should be started and stopped along with the browser.
TEST_F(SessionManagerProcessTest, LivenessCheckingStartStop) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(true);
  {
    Sequence start_stop;
    EXPECT_CALL(*liveness_checker_, Start()).Times(2);
    EXPECT_CALL(*liveness_checker_, Stop()).Times(AtLeast(1));
  }
  ExpectOneJobReRun(job, PackSignal(0));
  SimpleRunManager();
}

// If the child indicates it should be stopped, the session manager must honor.
TEST_F(SessionManagerProcessTest, MustStopChild) {
  FakeBrowserJob* job = CreateMockJobAndInitManager(true);
  ExpectLivenessChecking();
  EXPECT_CALL(*job, KillEverything(SIGKILL, _)).Times(AnyNumber());
  EXPECT_CALL(*job, ShouldStop()).WillOnce(Return(true));
  EXPECT_CALL(*session_manager_impl_, ScreenIsLocked())
      .WillRepeatedly(Return(false));

  SimpleRunManager();
}

TEST_F(SessionManagerProcessTest, TestWipeOnBadState) {
  CreateMockJobAndInitManager(true);

  EXPECT_CALL(*session_manager_impl_, Initialize()).WillOnce(Return(false));

  // Expect Powerwash to be triggered.
  EXPECT_CALL(*session_manager_impl_, InitiateDeviceWipe()).Times(1);
  EXPECT_CALL(*session_manager_impl_, Finalize()).Times(1);

  ASSERT_FALSE(manager_->test_api().InitializeImpl());
  ASSERT_EQ(SessionManagerService::MUST_WIPE_DEVICE, manager_->exit_code());
}

}  // namespace login_manager
