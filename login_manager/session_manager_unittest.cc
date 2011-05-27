// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <errno.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_temp_dir.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <crypto/rsa_private_key.h>

#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_mitigator.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/mock_upstart_signal_emitter.h"

namespace login_manager {
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::_;

using chromeos::Resetter;
using chromeos::glib::ScopedError;

static const char kCheckedFile[] = "/tmp/checked_file";
static const char kUptimeFile[] = "/tmp/uptime-chrome-exec";
static const char kDiskFile[] = "/tmp/disk-chrome-exec";

// compatible with void Run()
static void BadExit() { _exit(1); }
static void BadExitAfterSleep() { sleep(1); _exit(1); }
static void RunAndSleep() { while (true) { sleep(1); } };
static void CleanExit() { _exit(0); }

// Used as a base class for the tests in this file.
// Gives useful shared functionality.
class SessionManagerTest : public ::testing::Test {
 public:
  SessionManagerTest()
      : manager_(NULL),
        utils_(new MockSystemUtils),
        file_checker_(new MockFileChecker(kCheckedFile)),
        mitigator_(new MockMitigator),
        upstart_(new MockUpstartSignalEmitter),
        device_policy_service_(new MockDevicePolicyService),
        must_destroy_mocks_(true) {
  }

  virtual ~SessionManagerTest() {
    if (must_destroy_mocks_) {
      delete file_checker_;
      delete mitigator_;
      delete upstart_;
    }
    FilePath uptime(kUptimeFile);
    FilePath disk(kDiskFile);
    file_util::Delete(uptime, false);
    file_util::Delete(disk, false);
  }

  virtual void TearDown() {
    must_destroy_mocks_ = !manager_;
    manager_ = NULL;
  }

  void SimpleRunManager() {
    EXPECT_CALL(*device_policy_service_, Initialize())
        .WillOnce(Return(true));
    EXPECT_CALL(*device_policy_service_, PersistPolicySync())
        .WillOnce(Return(true));
    manager_->Run();
  }

 protected:
  // NOT const so that they can be passed to methods that implement dbus calls,
  // which (of necessity) take bare gchar*.
  static char kFakeEmail[];
  static const pid_t kDummyPid;

  // Creates the manager with the jobs. Mocks the file checker.
  // The second job can be NULL.
  void InitManager(MockChildJob* job1, MockChildJob* job2) {
    std::vector<ChildJobInterface*> jobs;
    EXPECT_CALL(*job1, GetName())
        .WillRepeatedly(Return(std::string("job1")));
    EXPECT_CALL(*job1, IsDesiredUidSet())
        .WillRepeatedly(Return(false));
    jobs.push_back(job1);
    if (job2) {
      EXPECT_CALL(*job2, GetName())
          .WillRepeatedly(Return(std::string("job2")));
      EXPECT_CALL(*job2, IsDesiredUidSet())
          .WillRepeatedly(Return(false));
      jobs.push_back(job2);
    }
    ASSERT_TRUE(MessageLoop::current() == NULL);
    manager_ = new SessionManagerService(jobs);
    manager_->set_file_checker(file_checker_);
    manager_->set_mitigator(mitigator_);
    manager_->test_api().set_exit_on_child_done(true);
    manager_->test_api().set_upstart_signal_emitter(upstart_);
    manager_->test_api().set_device_policy_service(device_policy_service_);
  }

  void MockUtils() {
    manager_->test_api().set_systemutils(utils_.release());
  }

  void ExpectStartSession(const std::string& email_string, MockChildJob* job) {
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);
    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(false),
                        Return(true)));
    // Confirm that the key is present.
    EXPECT_CALL(*device_policy_service_, KeyMissing())
        .WillOnce(Return(false));
  }

  void ExpectStartOwnerSession(const std::string& email_string) {
    MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);

    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(true),
                        Return(true)));

    // Confirm that the key is present.
    EXPECT_CALL(*device_policy_service_, KeyMissing())
        .WillOnce(Return(false));
  }

  void ExpectStartSessionUnowned(const std::string& email_string) {
    MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);

    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(false),
                        Return(true)));

    // Indidcate that there is no owner key in order to trigger a new one to be
    // generated.
    EXPECT_CALL(*device_policy_service_, KeyMissing())
        .WillOnce(Return(true));

    MockKeyGenerator* gen = new MockKeyGenerator;
    EXPECT_CALL(*gen, Start(_, _))
        .WillRepeatedly(Return(true));

    manager_->set_uid(getuid());
    manager_->test_api().set_keygen(gen);
  }

  void ExpectDeprecatedCall() {
    EXPECT_CALL(*(utils_.get()),
                SendStatusSignalToChromium(
                    chromium::kPropertyChangeCompleteSignal, false))
        .Times(1);
    MockUtils();
  }

  void StartFakeSession() {
    manager_->test_api().set_session_started(true, kFakeEmail);
  }

  enum ChildRuns {
    ALWAYS, NEVER, ONCE, EXACTLY_ONCE, TWICE, MAYBE_NEVER
  };

  // Configures the file_checker_ to run the children.
  void RunChildren(ChildRuns child_runs) {
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
    case ONCE:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(false))
          .WillOnce(Return(true))
          .RetiresOnSaturation();
      break;
    case EXACTLY_ONCE:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(false))
          .RetiresOnSaturation();
      break;
    case TWICE:
      EXPECT_CALL(*file_checker_, exists())
          .WillOnce(Return(false))
          .WillOnce(Return(false))
          .WillOnce(Return(true))
          .RetiresOnSaturation();
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

  // Caller takes ownership.
  GArray* CreateArray(const char* input, const int len) {
    GArray* output = g_array_new(FALSE, FALSE, 1);
    g_array_append_vals(output, input, len);
    return output;
  }

  scoped_refptr<SessionManagerService> manager_;
  scoped_ptr<MockSystemUtils> utils_;
  MockFileChecker* file_checker_;
  MockMitigator* mitigator_;
  MockUpstartSignalEmitter* upstart_;
  MockDevicePolicyService* device_policy_service_;
  bool must_destroy_mocks_;
};

// static
char SessionManagerTest::kFakeEmail[] = "cmasone@whaaat.org";
const pid_t SessionManagerTest::kDummyPid = 4;


TEST_F(SessionManagerTest, NoLoopTest) {
  MockChildJob* job = CreateTrivialMockJob(NEVER);
  SimpleRunManager();
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

  SimpleRunManager();
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

  SimpleRunManager();
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

  SimpleRunManager();
}

TEST_F(SessionManagerTest, CleanExitChild) {
  MockChildJob* job = CreateTrivialMockJob(EXACTLY_ONCE);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  SimpleRunManager();
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
  EXPECT_CALL(*job2, ShouldStop())
      .WillOnce(Return(true));

  SimpleRunManager();
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

  SimpleRunManager();
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

  SimpleRunManager();
}

TEST_F(SessionManagerTest, KeygenTest) {
  const char key_file_name[] = "foo.pub";
  ScopedTempDir tmpdir;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  FilePath key_file_path(tmpdir.path().AppendASCII(key_file_name));

  int pid = fork();
  if (pid == 0) {
    execl("./keygen", "./keygen", key_file_path.value().c_str(), NULL);
    exit(255);
  }
  int status;
  while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
    ;

  LOG(INFO) << "exited waitpid. " << pid << "\n"
            << "  WIFSIGNALED is " << WIFSIGNALED(status) << "\n"
            << "  WTERMSIG is " << WTERMSIG(status) << "\n"
            << "  WIFEXITED is " << WIFEXITED(status) << "\n"
            << "  WEXITSTATUS is " << WEXITSTATUS(status);

  EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  EXPECT_TRUE(file_util::PathExists(key_file_path));

  SystemUtils utils;
  int32 file_size = 0;
  EXPECT_TRUE(utils.EnsureAndReturnSafeFileSize(key_file_path, &file_size));
  EXPECT_GT(file_size, 0);
}

TEST_F(SessionManagerTest, SessionNotStartedCleanup) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  manager_->test_api().set_child_pid(0, kDummyPid);

  int timeout = 3;
  EXPECT_CALL(*(utils_.get()), kill(kDummyPid, getuid(), SIGKILL))
      .WillOnce(Return(0));
  EXPECT_CALL(*(utils_.get()), ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));
  MockUtils();

  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerTest, SessionNotStartedSlowKillCleanup) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  manager_->test_api().set_child_pid(0, kDummyPid);

  int timeout = 3;
  EXPECT_CALL(*(utils_.get()), kill(kDummyPid, getuid(), SIGKILL))
      .WillOnce(Return(0));
  EXPECT_CALL(*(utils_.get()), ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(*(utils_.get()), kill(kDummyPid, getuid(), SIGABRT))
      .WillOnce(Return(0));
  MockUtils();

  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerTest, SessionStartedCleanup) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  int timeout = 3;
  EXPECT_CALL(*(utils_.get()), kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(*(utils_.get()), ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));
  MockUtils();

  ExpectStartSession(email, job);
  ScopedError error;
  manager_->StartSession(email, nothing, &out, &Resetter(&error).lvalue());
  SimpleRunManager();
}

TEST_F(SessionManagerTest, SessionStartedSlowKillCleanup) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  int timeout = 3;
  EXPECT_CALL(*(utils_.get()), kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(*(utils_.get()), ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(*(utils_.get()), kill(kDummyPid, getuid(), SIGABRT))
      .WillOnce(Return(0));
  MockUtils();

  ExpectStartSession(email, job);
  ScopedError error;
  manager_->StartSession(email, nothing, &out, &Resetter(&error).lvalue());
  SimpleRunManager();
}

// Test that we avoid killing jobs that return true from their ShouldNeverKill()
// methods.
TEST_F(SessionManagerTest, HonorShouldNeverKill) {
  const int kNormalPid = 100;
  const int kShouldNeverKillPid = 101;
  const int kTimeout = 3;

  MockChildJob* normal_job = new MockChildJob;
  MockChildJob* should_never_kill_job = new MockChildJob;
  InitManager(normal_job, should_never_kill_job);

  manager_->test_api().set_child_pid(0, kNormalPid);
  manager_->test_api().set_child_pid(1, kShouldNeverKillPid);
  manager_->test_api().set_session_started(true, kFakeEmail);

  EXPECT_CALL(*should_never_kill_job, ShouldNeverKill())
      .WillRepeatedly(Return(true));

  // Say that the normal job died after the TERM signal.
  EXPECT_CALL(*(utils_.get()), ChildIsGone(kNormalPid, kTimeout))
      .WillRepeatedly(Return(true));

  // We should just see a TERM signal for the normal job.
  EXPECT_CALL(*(utils_.get()), kill(kNormalPid, getuid(), SIGTERM))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*(utils_.get()), kill(kNormalPid, getuid(), SIGABRT))
      .Times(0);
  EXPECT_CALL(*(utils_.get()), kill(kShouldNeverKillPid, getuid(), SIGTERM))
      .Times(0);
  EXPECT_CALL(*(utils_.get()), kill(kShouldNeverKillPid, getuid(), SIGABRT))
      .Times(0);

  MockUtils();
  manager_->test_api().CleanupChildren(kTimeout);
}

TEST_F(SessionManagerTest, StartSession) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSession(email, job);
  ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerTest, StartSessionNew) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSessionUnowned(email);
  ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerTest, StartSessionInvalidUser) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  gboolean out;
  gchar email[] = "user";
  gchar nothing[] = "";
  ScopedError error;
  EXPECT_EQ(FALSE, manager_->StartSession(email,
                                          nothing,
                                          &out,
                                          &Resetter(&error).lvalue()));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_INVALID_EMAIL, error->code);
}

TEST_F(SessionManagerTest, StartSessionDevicePolicyFailure) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ScopedError error;

  // Upon the owner login check, return an error.
  EXPECT_CALL(*device_policy_service_,
              CheckAndHandleOwnerLogin(StrEq(email), _, _))
      .WillOnce(Return(false));

  EXPECT_EQ(FALSE, manager_->StartSession(email,
                                          nothing,
                                          &out,
                                          &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerTest, StartOwnerSession) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartOwnerSession(email);
  ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerTest, StopSession) {
  MockChildJob* job = CreateTrivialMockJob(ALWAYS);
  gboolean out;
  gchar nothing[] = "";
  manager_->StopSession(nothing, &out, NULL);
}

TEST_F(SessionManagerTest, StatsRecorded) {
  FilePath uptime(kUptimeFile);
  FilePath disk(kDiskFile);
  file_util::Delete(uptime, false);
  file_util::Delete(disk, false);
  MockChildJob* job = CreateTrivialMockJob(ONCE);
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(false));
  ON_CALL(*job, GetName())
      .WillByDefault(Return(std::string("foo")));
  SimpleRunManager();
  LOG(INFO) << "Finished the run!";
  EXPECT_TRUE(file_util::PathExists(uptime));
  EXPECT_TRUE(file_util::PathExists(disk));
}

TEST_F(SessionManagerTest, DeprecatedMethod) {
  MockChildJob* owned_by_manager_ = CreateTrivialMockJob(MAYBE_NEVER);
  ExpectDeprecatedCall();
  ScopedError error;
  EXPECT_EQ(FALSE, manager_->DeprecatedError("", &Resetter(&error).lvalue()));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PROPERTY, error->code);
  SimpleRunManager();
}

TEST_F(SessionManagerTest, SetOwnerKeyShouldFail) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()),
              SendStatusSignalToChromium(chromium::kOwnerKeySetSignal, false))
      .Times(1);
  MockUtils();

  GError* error = NULL;
  const char fake_key_data[] = "fake_key";
  GArray* fake_key = CreateArray(fake_key_data, strlen(fake_key_data));
  EXPECT_EQ(FALSE, manager_->SetOwnerKey(fake_key, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY, error->code);
  g_array_free(fake_key, TRUE);
  g_error_free(error);
}

TEST_F(SessionManagerTest, EnableChromeTesting) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()), kill(-kDummyPid, getuid(), SIGKILL))
      .Times(2).WillOnce(Return(0)).WillOnce(Return(0));
  MockUtils();

  EXPECT_CALL(*job, GetName())
      .WillRepeatedly(Return("chrome"));
  EXPECT_CALL(*job, SetExtraArguments(_))
      .Times(1);
  EXPECT_CALL(*job, RecordTime())
      .Times(2);
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  // DBus arrays are null-terminated.
  char* args1[] = {"--repeat-arg", "--one-time-arg", NULL};
  char* args2[] = {"--dummy", "--repeat-arg", NULL};
  gchar* testing_path, *file_path;

  manager_->test_api().set_child_pid(0, kDummyPid);
  EXPECT_TRUE(manager_->EnableChromeTesting(false, args1, &testing_path, NULL));

  // Now that we have the testing channel we can predict the
  // arguments that will be passed to SetExtraArguments().
  // We should see the same testing channel path in the subsequent invocation.
  std::string testing_argument = "--testing-channel=NamedTestingInterface:";
  testing_argument.append(testing_path);
  std::vector<std::string> extra_arguments(args2, args2 + arraysize(args2) - 1);
  extra_arguments.push_back(testing_argument);
  EXPECT_CALL(*job, SetExtraArguments(extra_arguments))
      .Times(1);

  // This invocation should do everything again, since force_relaunch is true.
  manager_->test_api().set_child_pid(0, kDummyPid);
  EXPECT_TRUE(manager_->EnableChromeTesting(true, args2, &file_path, NULL));
  EXPECT_EQ(std::string(testing_path), std::string(file_path));

  // This invocation should do nothing.
  manager_->test_api().set_child_pid(0, kDummyPid);
  EXPECT_TRUE(manager_->EnableChromeTesting(false, args2, &file_path, NULL));
  EXPECT_EQ(std::string(testing_path), std::string(file_path));
}

TEST_F(SessionManagerTest, StorePolicyNoSession) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  int flags = PolicyService::KEY_ROTATE |
              PolicyService::KEY_INSTALL_NEW |
              PolicyService::KEY_CLOBBER;
  EXPECT_CALL(*device_policy_service_, Store(policy_blob, NULL, flags))
      .WillOnce(Return(TRUE));
  EXPECT_EQ(TRUE, manager_->StorePolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerTest, StorePolicySessionStarted) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  manager_->test_api().set_session_started(true, "user@somewhere");
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  int flags = PolicyService::KEY_ROTATE;
  EXPECT_CALL(*device_policy_service_, Store(policy_blob, NULL, flags))
      .WillOnce(Return(TRUE));
  EXPECT_EQ(TRUE, manager_->StorePolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerTest, RetrievePolicy) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  EXPECT_CALL(*device_policy_service_, Retrieve(_, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_blob),
                      Return(TRUE)));
  GArray* out_blob;
  ScopedError error;
  EXPECT_EQ(TRUE, manager_->RetrievePolicy(&out_blob,
                                           &Resetter(&error).lvalue()));
  EXPECT_EQ(out_blob, policy_blob);
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerTest, RestartJobUnknownPid) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gint pid = kDummyPid + 1;
  gchar arguments[] = "";
  ScopedError error;
  EXPECT_EQ(FALSE, manager_->RestartJob(pid, arguments, &out,
                                        &Resetter(&error).lvalue()));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PID, error->code);
  EXPECT_EQ(FALSE, out);
}

TEST_F(SessionManagerTest, RestartJob) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  manager_->test_api().set_child_pid(0, kDummyPid);
  EXPECT_CALL(*(utils_.get()), kill(-kDummyPid, getuid(), SIGKILL))
      .WillOnce(Return(0));
  MockUtils();

  EXPECT_CALL(*job, GetName())
      .WillRepeatedly(Return(std::string("chrome")));
  EXPECT_CALL(*job, SetArguments("dummy"))
      .Times(1);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  std::string email_string("");
  ExpectStartSession(email_string, job);
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  gboolean out;
  gint pid = kDummyPid;
  gchar arguments[] = "dummy";
  EXPECT_EQ(TRUE, manager_->RestartJob(pid, arguments, &out, NULL));
  EXPECT_EQ(TRUE, out);
}

TEST_F(SessionManagerTest, RestartJobWrongPid) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gint pid = kDummyPid;
  gchar arguments[] = "dummy";
  ScopedError error;
  EXPECT_EQ(FALSE, manager_->RestartJob(pid, arguments, &out,
                                        &Resetter(&error).lvalue()));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PID, error->code);
  EXPECT_EQ(FALSE, out);
}

}  // namespace login_manager
