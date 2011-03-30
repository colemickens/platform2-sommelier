// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <errno.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include <base/base64.h>
#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/crypto/rsa_private_key.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/ref_counted.h>
#include <base/scoped_ptr.h>
#include <base/scoped_temp_dir.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "login_manager/bindings/client.h"
#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_device_policy.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_mitigator.h"
#include "login_manager/mock_nss_util.h"
#include "login_manager/mock_owner_key.h"
#include "login_manager/mock_pref_store.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/mock_upstart_signal_emitter.h"
#include "login_manager/system_utils.h"

namespace login_manager {

using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::_;

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

  virtual void SetUp() {
    property_ = StringPrintf("%s=%s", kPropName, kPropValue);
    char sig[] = "signature";
    int len = strlen(sig);
    sig[2] = '\0';  // to make sure we can handle NULL inside a signature.
    fake_sig_ = CreateArray(sig, len);
    ASSERT_TRUE(base::Base64Encode(std::string(fake_sig_->data, fake_sig_->len),
                                   &fake_sig_encoded_));
  }

  virtual void TearDown() {
    // just to be sure...
    NssUtil::set_factory(NULL);
    g_array_free(fake_sig_, TRUE);
    must_destroy_mocks_ = !manager_;
    manager_ = NULL;
  }

  void SimpleRunManager(MockPrefStore* store) {
    SimpleRunManager(store, new MockDevicePolicy);
  }

  void SimpleRunManager(MockPrefStore* store, MockDevicePolicy* policy) {
    EXPECT_CALL(*store, Persist())
        .WillOnce(Return(true));
    manager_->test_api().set_prefstore(store);
    EXPECT_CALL(*policy, Persist())
        .WillOnce(Return(true));
    manager_->test_api().set_policy(policy);
    manager_->Run();
  }

 protected:
  // NOT const so that they can be passed to methods that implement dbus calls,
  // which (of necessity) take bare gchar*.
  static char kFakeEmail[];
  static char kPropName[];
  static char kPropValue[];
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
  }

  void MockUtils() {
    manager_->test_api().set_systemutils(utils_.release());
  }

  void ExpectStartSession(const std::string& email_string,
                          MockChildJob* job,
                          MockPrefStore* store) {
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);
    MockOwnerKey* key = new MockOwnerKey;
    EXPECT_CALL(*key, PopulateFromDiskIfPossible())
        .WillRepeatedly(Return(true));
    // First, expect an attempt to set the device owner property, but
    // act like this user isn't the owner.
    EXPECT_CALL(*key, Sign(_, _, _))
        .WillOnce(Return(false));
    manager_->test_api().set_ownerkey(key);
    // Now, expect an attempt to check whether this user is the owner; respond
    // as though he is not.
    std::string other_user("notme");
    EXPECT_CALL(*store, Get(_, _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(other_user),
                        Return(true)));
    EXPECT_CALL(*key, Verify(_, _, _, _))
        .WillOnce(Return(true));
    // Confirm that the device is owned.
    EXPECT_CALL(*key, HaveCheckedDisk())
        .WillOnce(Return(true));
    EXPECT_CALL(*key, IsPopulated())
        .WillOnce(Return(true));
  }

  void ExpectStartSessionUnowned(const std::string& email_string,
                                 MockPrefStore* store) {
    MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);

    MockChildJob* k_job = new MockChildJob;
    EXPECT_CALL(*k_job, SetDesiredUid(getuid()))
        .Times(1);
    EXPECT_CALL(*k_job, GetDesiredUid())
        .Times(1)
        .WillRepeatedly(Return(getuid()));
    EXPECT_CALL(*k_job, IsDesiredUidSet())
        .WillRepeatedly(Return(true));
    ON_CALL(*k_job, Run())
        .WillByDefault(Invoke(CleanExit));
    int keygen_pid = kDummyPid + 1;

    MockOwnerKey* key = new MockOwnerKey;
    EXPECT_CALL(*key, PopulateFromDiskIfPossible())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*key, StartGeneration(k_job))
        .WillOnce(Return(keygen_pid));
    // act like this user isn't the owner.
    EXPECT_CALL(*key, Sign(_, _, _))
        .WillOnce(Return(false));

    // Now, expect an attempt to check whether this user is the owner; respond
    // as though there isn't one.
    EXPECT_CALL(*store, Get(_, _, _))
        .WillOnce(Return(false));
    // Confirm that the device is NOT owned.
    EXPECT_CALL(*key, HaveCheckedDisk())
        .WillOnce(Return(true));
    EXPECT_CALL(*key, IsPopulated())
        .WillOnce(Return(false));

    manager_->test_api().set_keygen_job(k_job);  // manager_ takes ownership.
    manager_->set_uid(getuid());
    manager_->test_api().set_ownerkey(key);
    manager_->test_api().set_prefstore(store);

    EXPECT_CALL(*(utils_.get()), kill(keygen_pid, getuid(), SIGTERM))
        .WillOnce(Return(0));
    EXPECT_CALL(*(utils_.get()), ChildIsGone(keygen_pid, _))
        .WillOnce(Return(true));
    MockUtils();
  }

  void ExpectStartSessionForOwner(const std::string& email_string,
                                  MockOwnerKey* key,
                                  MockPrefStore* store) {
    ON_CALL(*key, PopulateFromDiskIfPossible())
        .WillByDefault(Return(true));
    // First, mimic attempt to whitelist the owner and set a the
    // device owner pref.
    EXPECT_CALL(*key, Sign(_, _, _))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    EXPECT_CALL(*store, Set(_, email_string, _))
        .Times(1);
    EXPECT_CALL(*key, Sign(StrEq(email_string), email_string.length(), _))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    EXPECT_CALL(*store, Whitelist(email_string, _))
        .Times(1);
    EXPECT_CALL(*store, Persist())
        .WillOnce(Return(true))
        .WillOnce(Return(true))
        .WillOnce(Return(true));
    // Now, expect an attempt to check whether this user is the owner;
    // respond as though he is.
    EXPECT_CALL(*store, Get(_, _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(email_string),
                        Return(true)));
    EXPECT_CALL(*key, Verify(_, _, _, _))
        .WillOnce(Return(true));
    // Confirm that the device is owned.
    EXPECT_CALL(*key, HaveCheckedDisk())
        .Times(AtMost(1))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*key, IsPopulated())
        .Times(AtMost(1))
        .WillRepeatedly(Return(true));
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
  bool must_destroy_mocks_;
  std::string property_;
  GArray* fake_sig_;
  std::string fake_sig_encoded_;
};

// static
char SessionManagerTest::kFakeEmail[] = "cmasone@whaaat.org";
char SessionManagerTest::kPropName[] = "name";
char SessionManagerTest::kPropValue[] = "value";
const pid_t SessionManagerTest::kDummyPid = 4;


TEST_F(SessionManagerTest, NoLoopTest) {
  MockChildJob* job = CreateTrivialMockJob(NEVER);
  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
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

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
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

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
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

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
}

TEST_F(SessionManagerTest, CleanExitChild) {
  MockChildJob* job = CreateTrivialMockJob(EXACTLY_ONCE);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  EXPECT_CALL(*job, ShouldStop())
      .WillOnce(Return(true));
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
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

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
}

TEST_F(SessionManagerTest, LoadOwnerKey) {
  MockChildJob* job = CreateTrivialMockJob(ONCE);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  ON_CALL(*job, Run())
      .WillByDefault(Invoke(CleanExit));

  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));

  manager_->test_api().set_ownerkey(key);

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
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

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
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

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
}

TEST_F(SessionManagerTest, KeygenTest) {
  const char key_file_name[] = "foo.pub";
  ScopedTempDir tmpdir;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  FilePath key_file_path(tmpdir.path().AppendASCII(key_file_name));

  int pid = fork();
  if (pid == 0) {
    execl("./keygen", "./keygen", key_file_path.value().c_str(), NULL);
    exit(1);
  }
  int status;
  while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
    ;

  LOG(INFO) << "exited waitpid. " << pid << "\n"
            << "  WIFSIGNALED is " << WIFSIGNALED(status) << "\n"
            << "  WTERMSIG is " << WTERMSIG(status) << "\n"
            << "  WIFEXITED is " << WIFEXITED(status) << "\n"
            << "  WEXITSTATUS is " << WEXITSTATUS(status);

  ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  ASSERT_TRUE(file_util::PathExists(key_file_path));

  SystemUtils utils;
  int32 file_size = 0;
  ASSERT_TRUE(utils.EnsureAndReturnSafeFileSize(key_file_path, &file_size));
  ASSERT_GT(file_size, 0);
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

  MockPrefStore* store = new MockPrefStore;
  ExpectStartSession(email, job, store);
  manager_->test_api().set_prefstore(store);
  manager_->StartSession(email, nothing, &out, NULL);
  SimpleRunManager(store);
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

  MockPrefStore* store = new MockPrefStore;
  ExpectStartSession(email, job, store);
  manager_->test_api().set_prefstore(store);
  manager_->StartSession(email, nothing, &out, NULL);
  SimpleRunManager(store);
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
  MockPrefStore* store = new MockPrefStore;
  ExpectStartSession(email, job, store);
  manager_->test_api().set_prefstore(store);
  manager_->StartSession(email, nothing, &out, NULL);
}

TEST_F(SessionManagerTest, StartSessionNew) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  MockPrefStore* store = new MockPrefStore;
  ExpectStartSessionUnowned(email, store);
  chromeos::glib::ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &chromeos::Resetter(&error).lvalue()));

  SimpleRunManager(store);
}

TEST_F(SessionManagerTest, StartOwnerSession) {
  MockFactory<KeyCheckUtil> factory;
  NssUtil::set_factory(&factory);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";

  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*job, StartSession(email))
      .Times(1);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kPropertyChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kWhitelistChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  MockUtils();

  MockOwnerKey* key = new MockOwnerKey;
  MockPrefStore* store = new MockPrefStore;
  MockDevicePolicy* policy = new MockDevicePolicy;
  ExpectStartSessionForOwner(email, key, store);
  EXPECT_CALL(*policy, Persist())
      .WillOnce(Return(true));

  manager_->test_api().set_ownerkey(key);
  manager_->test_api().set_prefstore(store);
  manager_->test_api().set_policy(policy);

  manager_->StartSession(email, nothing, &out, NULL);
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  manager_->Run();
}

TEST_F(SessionManagerTest, StartOwnerSessionNoKeyNoRecover) {
  MockFactory<KeyFailUtil> factory;
  NssUtil::set_factory(&factory);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";

  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kPropertyChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kWhitelistChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  MockUtils();

  EXPECT_CALL(*mitigator_, Mitigate())
      .WillOnce(Return(false));
  MockOwnerKey* key = new MockOwnerKey;
  MockPrefStore* store = new MockPrefStore;
  MockDevicePolicy* policy = new MockDevicePolicy;
  ExpectStartSessionForOwner(email, key, store);
  EXPECT_CALL(*policy, Persist())
      .WillOnce(Return(true));

  manager_->test_api().set_ownerkey(key);
  manager_->test_api().set_prefstore(store);
  manager_->test_api().set_policy(policy);

  bool ret_code = manager_->StartSession(email, nothing, &out, NULL);
  EXPECT_FALSE(ret_code);
  EXPECT_EQ(ret_code, out);
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  manager_->Run();
}

TEST_F(SessionManagerTest, StartSessionError) {
  MockChildJob* job = CreateTrivialMockJob(ALWAYS);
  gboolean out;
  gchar email[] = "user";
  gchar nothing[] = "";
  GError* error = NULL;
  manager_->StartSession(email, nothing, &out, &error);
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_INVALID_EMAIL, error->code);
  g_error_free(error);
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
  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
  LOG(INFO) << "Finished the run!";
  EXPECT_TRUE(file_util::PathExists(uptime));
  EXPECT_TRUE(file_util::PathExists(disk));
}

TEST_F(SessionManagerTest, SetOwnerKeyShouldFail) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kOwnerKeySetSignal,
                                   StrEq("failure")))
      .Times(1);
  MockUtils();

  GError* error = NULL;
  GArray* fake_key = CreateArray(kPropValue, strlen(kPropValue));
  EXPECT_EQ(FALSE, manager_->SetOwnerKey(fake_key, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY, error->code);
  g_array_free(fake_key, TRUE);
  g_error_free(error);
}

TEST_F(SessionManagerTest, ValidateAndStoreOwnerKey) {
  MockFactory<KeyCheckUtil> factory;
  NssUtil::set_factory(&factory);

  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kOwnerKeySetSignal,
                                   StrEq("success")))
      .Times(1);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kPropertyChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kWhitelistChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  MockUtils();

  std::vector<uint8> pub_key;
  NssUtil::KeyFromBuffer(kPropValue, &pub_key);

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  MockDevicePolicy* policy = new MockDevicePolicy;

  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, PopulateFromBuffer(pub_key))
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Sign(_, _, _))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Persist())
      .WillOnce(Return(true));
  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, Whitelist(kFakeEmail, _))
      .Times(1);
  EXPECT_CALL(*store, Set(_, kFakeEmail, _))
      .Times(1);
  EXPECT_CALL(*store, Persist())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  manager_->test_api().set_prefstore(store);

  EXPECT_CALL(*policy, Persist())
      .WillOnce(Return(true));
  manager_->test_api().set_policy(policy);

  StartFakeSession();
  manager_->ValidateAndStoreOwnerKey(kPropValue);
  manager_->Run();
}

TEST_F(SessionManagerTest, WhitelistNoKey) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(false));
  manager_->test_api().set_ownerkey(key);

  GError* error = NULL;
  EXPECT_EQ(FALSE, manager_->Whitelist(kFakeEmail, fake_sig_, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_NO_OWNER_KEY, error->code);
  g_error_free(error);

  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
}

TEST_F(SessionManagerTest, WhitelistVerifyFail) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Verify(StrEq(kFakeEmail), strlen(kFakeEmail),
                           fake_sig_->data, fake_sig_->len))
      .WillOnce(Return(false));
  manager_->test_api().set_ownerkey(key);

  GError* error = NULL;
  EXPECT_EQ(FALSE, manager_->Whitelist(kFakeEmail, fake_sig_, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_VERIFY_FAIL, error->code);
  g_error_free(error);
  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
}

TEST_F(SessionManagerTest, Whitelist) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kWhitelistChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  MockUtils();

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  MockDevicePolicy* policy = new MockDevicePolicy;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Verify(StrEq(kFakeEmail), strlen(kFakeEmail),
                           fake_sig_->data, fake_sig_->len))
      .WillOnce(Return(true));

  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, Whitelist(kFakeEmail, _))
      .Times(1);
  EXPECT_CALL(*store, Persist())
      .WillOnce(Return(true))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy, Persist())
      .WillOnce(Return(true));
  manager_->test_api().set_policy(policy);

  manager_->test_api().set_prefstore(store);
  EXPECT_EQ(TRUE, manager_->Whitelist(kFakeEmail, fake_sig_, NULL));
  manager_->Run();
}

TEST_F(SessionManagerTest, Unwhitelist) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kWhitelistChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  MockUtils();

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  MockDevicePolicy* policy = new MockDevicePolicy;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Verify(StrEq(kFakeEmail), strlen(kFakeEmail),
                           fake_sig_->data, fake_sig_->len))
      .WillOnce(Return(true));

  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, Unwhitelist(kFakeEmail))
      .Times(1);
  EXPECT_CALL(*store, Persist())
      .WillOnce(Return(true))
      .WillOnce(Return(true));

  EXPECT_CALL(*policy, Persist())
      .WillOnce(Return(true));
  manager_->test_api().set_policy(policy);

  manager_->test_api().set_prefstore(store);
  EXPECT_EQ(TRUE, manager_->Unwhitelist(kFakeEmail, fake_sig_, NULL));
  manager_->Run();
}

TEST_F(SessionManagerTest, CheckWhitelistFail) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, GetFromWhitelist(kFakeEmail, _))
      .WillOnce(Return(false));
  manager_->test_api().set_prefstore(store);

  GError* error = NULL;
  GArray* out_sig = NULL;
  EXPECT_EQ(FALSE, manager_->CheckWhitelist(kFakeEmail, &out_sig, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_ILLEGAL_USER, error->code);
  g_error_free(error);
  SimpleRunManager(store);
}

TEST_F(SessionManagerTest, CheckWhitelist) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, GetFromWhitelist(kFakeEmail, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(fake_sig_encoded_),
                      Return(true)))
      .RetiresOnSaturation();
  manager_->test_api().set_prefstore(store);

  GArray* out_sig = NULL;
  ASSERT_EQ(TRUE, manager_->CheckWhitelist(kFakeEmail, &out_sig, NULL));

  int i = 0;
  for (i = 0; i < fake_sig_->len; i++) {
    EXPECT_EQ(g_array_index(fake_sig_, uint8, i),
              g_array_index(out_sig, uint8, i));
  }
  EXPECT_EQ(i, fake_sig_->len);
  g_array_free(out_sig, false);
  SimpleRunManager(store);
}

TEST_F(SessionManagerTest, EnumerateWhitelisted) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  std::vector<std::string> fake_whitelisted;
  fake_whitelisted.push_back("who.am.i@youface.com");
  fake_whitelisted.push_back("i.am.you@youface.net");
  fake_whitelisted.push_back("you.are.me@youface.org");

  MockPrefStore* store = new MockPrefStore;
  EXPECT_CALL(*store, EnumerateWhitelisted(_))
      .WillOnce(SetArgumentPointee<0>(fake_whitelisted))
      .RetiresOnSaturation();
  manager_->test_api().set_prefstore(store);

  char** out_list = NULL;
  ASSERT_EQ(TRUE, manager_->EnumerateWhitelisted(&out_list, NULL));

  int i;
  for (i = 0; out_list[i] != NULL; ++i) {
    EXPECT_EQ(fake_whitelisted[i], out_list[i]);
  }
  EXPECT_EQ(fake_whitelisted.size(), i);
  g_strfreev(out_list);
}

TEST_F(SessionManagerTest, EnumerateEmptyWhitelist) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  std::vector<std::string> fake_whitelisted;

  MockPrefStore* store = new MockPrefStore;
  EXPECT_CALL(*store, EnumerateWhitelisted(_))
      .WillOnce(SetArgumentPointee<0>(fake_whitelisted))
      .RetiresOnSaturation();
  manager_->test_api().set_prefstore(store);

  char** out_list = NULL;
  ASSERT_EQ(TRUE, manager_->EnumerateWhitelisted(&out_list, NULL));

  int i;
  for (i = 0; out_list[i] != NULL; ++i) {
    EXPECT_EQ(fake_whitelisted[i], out_list[i]);
  }
  EXPECT_EQ(fake_whitelisted.size(), i);
  g_strfreev(out_list);
}

TEST_F(SessionManagerTest, StorePolicy) {
  enterprise_management::PolicyFetchResponse fake_policy;
  fake_policy.set_policy_data(kPropName);
  fake_policy.set_policy_data_signature(kPropValue);
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  MockDevicePolicy* policy = new MockDevicePolicy;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Verify(StrEq(kPropName), strlen(kPropName),
                           StrEq(kPropValue), strlen(kPropValue)))
      .WillOnce(Return(true));
  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, Persist())
      .WillOnce(Return(true));
  manager_->test_api().set_prefstore(store);

  EXPECT_CALL(*policy, Persist())
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  EXPECT_CALL(*policy, Set(_))
      .Times(1);
  manager_->test_api().set_policy(policy);

  std::string pol_str = fake_policy.SerializeAsString();
  ASSERT_FALSE(pol_str.empty());
  GArray* policy_array = CreateArray(pol_str.c_str(), pol_str.length());
  EXPECT_EQ(TRUE, manager_->StorePolicy(policy_array, NULL));
  manager_->Run();
  g_array_free(policy_array, TRUE);
}

TEST_F(SessionManagerTest, StorePropertyNoKey) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(false));
  manager_->test_api().set_ownerkey(key);

  GError* error = NULL;
  EXPECT_EQ(FALSE, manager_->StoreProperty(kPropName,
                                           kPropValue,
                                           fake_sig_,
                                           &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_NO_OWNER_KEY, error->code);
  g_error_free(error);
  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
}

TEST_F(SessionManagerTest, StorePropertyVerifyFail) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Verify(StrEq(property_), property_.length(),
                           fake_sig_->data, fake_sig_->len))
      .WillOnce(Return(false));
  manager_->test_api().set_ownerkey(key);

  GError* error = NULL;
  EXPECT_EQ(FALSE, manager_->StoreProperty(kPropName,
                                           kPropValue,
                                           fake_sig_,
                                           &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_VERIFY_FAIL, error->code);
  g_error_free(error);
  SimpleRunManager(new MockPrefStore, new MockDevicePolicy);
}

TEST_F(SessionManagerTest, StoreProperty) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  EXPECT_CALL(*(utils_.get()),
              SendSignalToChromium(chromium::kPropertyChangeCompleteSignal,
                                   StrEq("success")))
      .Times(1);
  MockUtils();

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  MockDevicePolicy* policy = new MockDevicePolicy;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, IsPopulated())
      .WillOnce(Return(true));
  EXPECT_CALL(*key, Verify(StrEq(property_), property_.length(),
                           fake_sig_->data, fake_sig_->len))
      .WillOnce(Return(true));
  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, Set(kPropName, kPropValue, _))
      .Times(1);
  EXPECT_CALL(*store, Persist())
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  manager_->test_api().set_prefstore(store);

  EXPECT_CALL(*policy, Persist())
      .WillOnce(Return(true));
  manager_->test_api().set_policy(policy);

  EXPECT_EQ(TRUE, manager_->StoreProperty(kPropName,
                                          kPropValue,
                                          fake_sig_,
                                          NULL));
  manager_->Run();
}

TEST_F(SessionManagerTest, RetrievePropertyFail) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  manager_->test_api().set_ownerkey(key);

  EXPECT_CALL(*store, Get(kPropName, _, _))
      .WillOnce(Return(false));
  manager_->test_api().set_prefstore(store);

  chromeos::glib::ScopedError error;
  GArray* out_sig = NULL;
  gchar* out_value = NULL;
  EXPECT_EQ(FALSE,
            manager_->RetrieveProperty(kPropName,
                                       &out_value,
                                       &out_sig,
                                       &chromeos::Resetter(&error).lvalue()));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PROPERTY, error->code);
  if (out_sig)
    g_array_free(out_sig, false);
  if (out_value)
    g_free(out_value);
  SimpleRunManager(store);
}

TEST_F(SessionManagerTest, RetrieveProperty) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();

  MockPrefStore* store = new MockPrefStore;
  MockOwnerKey* key = new MockOwnerKey;
  EXPECT_CALL(*key, PopulateFromDiskIfPossible())
      .WillOnce(Return(true));
  manager_->test_api().set_ownerkey(key);

  std::string value(kPropValue);
  EXPECT_CALL(*store, Get(kPropName, _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(value),
                      SetArgumentPointee<2>(fake_sig_encoded_),
                      Return(true)))
      .RetiresOnSaturation();
  manager_->test_api().set_prefstore(store);

  GArray* out_sig = NULL;
  gchar* out_value = NULL;
  EXPECT_EQ(TRUE, manager_->RetrieveProperty(kPropName,
                                             &out_value,
                                             &out_sig,
                                             NULL));
  int i = 0;
  for (i = 0; i < fake_sig_->len; i++) {
    EXPECT_EQ(g_array_index(fake_sig_, uint8, i),
              g_array_index(out_sig, uint8, i));
  }
  EXPECT_EQ(i, fake_sig_->len);
  EXPECT_EQ(value, std::string(out_value));
  if (out_sig)
    g_array_free(out_sig, false);
  if (out_value)
    g_free(out_value);
  SimpleRunManager(store);
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

TEST_F(SessionManagerTest, RestartJobUnknownPid) {
  MockChildJob* job = CreateTrivialMockJob(MAYBE_NEVER);
  MockUtils();
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gint pid = kDummyPid + 1;
  gchar arguments[] = "";
  GError* error = NULL;
  EXPECT_EQ(FALSE, manager_->RestartJob(pid, arguments, &out, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PID, error->code);
  g_error_free(error);
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
  MockPrefStore* store = new MockPrefStore;
  ExpectStartSession(email_string, job, store);
  manager_->test_api().set_prefstore(store);
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
  GError* error = NULL;
  EXPECT_EQ(FALSE, manager_->RestartJob(pid, arguments, &out, &error));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PID, error->code);
  g_error_free(error);
  EXPECT_EQ(FALSE, out);
}

TEST(SessionManagerTestStatic, EmailAddressTest) {
  const char valid[] = "user_who+we.like@some-where.com";
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
