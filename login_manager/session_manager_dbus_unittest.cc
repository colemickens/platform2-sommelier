// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_unittest.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "login_manager/child_job.h"
#include "login_manager/device_management_backend.pb.h"
#include "login_manager/file_checker.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_child_process.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_system_utils.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::_;

using chromeos::Resetter;
using chromeos::glib::ScopedError;

namespace {

MATCHER_P(CastEq, str, "") {
  return std::equal(str.begin(), str.end(), reinterpret_cast<const char*>(arg));
}

}  // namespace

namespace login_manager {

// Used as a fixture for the tests in this file.
// Gives useful shared functionality.
class SessionManagerDBusTest : public SessionManagerTest {
 public:
  SessionManagerDBusTest() {}

  virtual ~SessionManagerDBusTest() {}

 protected:
  void ExpectSessionBoilerplate(const std::string& email_string,
                                bool guest,
                                bool for_owner,
                                MockChildJob* job) {
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);
    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(for_owner),
                        Return(true)));
    // Confirm that the key is present.
    EXPECT_CALL(*device_policy_service_, KeyMissing())
        .WillOnce(Return(false));

    ExpectUserPolicySetup();

    EXPECT_CALL(*metrics_, SendLoginUserType(false, guest, for_owner))
        .Times(1);

    EXPECT_CALL(utils_,
                BroadcastSignal(_, _, SessionManagerService::kStarted, _))
        .Times(1);
    EXPECT_CALL(utils_, IsDevMode())
        .WillOnce(Return(false));
  }

  void ExpectChildJobClearOneTimeArgument(MockChildJob* job) {
    EXPECT_CALL(*job, ClearOneTimeArgument())
        .Times(1);
  }

  void ExpectGuestSession(const std::string& email_string, MockChildJob* job) {
    ExpectSessionBoilerplate(email_string, true, false, job);
  }

  void ExpectStartSession(const std::string& email_string, MockChildJob* job) {
    ExpectSessionBoilerplate(email_string, false, false, job);
  }

  void ExpectStartOwnerSession(const std::string& email_string) {
    MockChildJob* job = CreateTrivialMockJob();
    ExpectSessionBoilerplate(email_string, false, true, job);
  }

  void ExpectStartSessionUnowned(const std::string& email_string) {
    MockChildJob* job = CreateTrivialMockJob();
    EXPECT_CALL(*job, StartSession(email_string))
        .Times(1);

    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(false),
                        Return(true)));

    // Indicate that there is no owner key in order to trigger a new one to be
    // generated.
    EXPECT_CALL(*device_policy_service_, KeyMissing())
        .WillOnce(Return(true));

    MockKeyGenerator* gen = new MockKeyGenerator;
    EXPECT_CALL(*gen, Start(_, _))
        .WillRepeatedly(Return(true));

    manager_->set_uid(getuid());
    manager_->test_api().set_keygen(gen);

    ExpectUserPolicySetup();

    EXPECT_CALL(utils_,
                BroadcastSignal(_, _, SessionManagerService::kStarted, _))
        .Times(1);
    EXPECT_CALL(utils_, IsDevMode())
        .WillOnce(Return(false));
  }

  void ExpectStorePolicy(MockDevicePolicyService* service,
                         const std::string& policy,
                         int flags) {
    EXPECT_CALL(*service, Store(CastEq(policy), policy.size(), _, flags))
        .WillOnce(Return(true));
  }

  void StartFakeSession() {
    manager_->test_api().set_session_started(true, kFakeEmail);
  }

  // Creates one job and initializes |manager_| with it, using the flag-file
  // mechanism to ensure that it only runs once.
  // Returns a pointer to the fake job for further mocking purposes.
  MockChildJob* CreateTrivialMockJob() {
    MockChildJob* job = new MockChildJob();
    InitManager(job, NULL);
    EXPECT_CALL(*file_checker_, exists())
        .Times(AnyNumber())
        .WillOnce(Return(true));
    return job;
  }

  // Creates one job and initializes |manager_| with it, using the flag-file
  // mechanism to ensure that it only runs once.
  void TrivialInitManager() {
    InitManager(new MockChildJob(), NULL);
    EXPECT_CALL(*file_checker_, exists())
        .Times(AnyNumber())
        .WillOnce(Return(true));
  }

  // Caller takes ownership.
  GArray* CreateArray(const char* input, const int len) {
    GArray* output = g_array_new(FALSE, FALSE, 1);
    g_array_append_vals(output, input, len);
    return output;
  }

  FilePath machine_info_file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerDBusTest);
};

TEST_F(SessionManagerDBusTest, SessionNotStartedCleanup) {
  TrivialInitManager();
  manager_->test_api().set_child_pid(0, kDummyPid);

  int timeout = 3;
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));
  MockUtils();

  manager_->test_api().CleanupChildren(timeout);
}

TEST_F(SessionManagerDBusTest, SessionNotStartedSlowKillCleanup) {
  TrivialInitManager();
  manager_->test_api().set_child_pid(0, kDummyPid);

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

TEST_F(SessionManagerDBusTest, SessionStartedCleanup) {
  MockChildJob* job = CreateTrivialMockJob();
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  int timeout = 3;
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(true));
  EXPECT_CALL(utils_,
              BroadcastSignal(_, _, SessionManagerService::kStopping, _))
      .Times(1);
  EXPECT_CALL(utils_,
              BroadcastSignal(_, _, SessionManagerService::kStopped, _))
      .Times(1);

  ExpectPolicySetup();
  ExpectStartSession(email, job);
  MockUtils();

  ScopedError error;
  manager_->StartSession(email, nothing, &out, &Resetter(&error).lvalue());
  manager_->Run();
}

TEST_F(SessionManagerDBusTest, SessionStartedSlowKillCleanup) {
  MockChildJob* job = CreateTrivialMockJob();
  manager_->test_api().set_child_pid(0, kDummyPid);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  int timeout = 3;
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGTERM))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_, ChildIsGone(kDummyPid, timeout))
      .WillOnce(Return(false));
  EXPECT_CALL(utils_, kill(kDummyPid, getuid(), SIGABRT))
      .WillOnce(Return(0));
  EXPECT_CALL(utils_,
              BroadcastSignal(_, _, SessionManagerService::kStopping, _))
      .Times(1);
  EXPECT_CALL(utils_,
              BroadcastSignal(_, _, SessionManagerService::kStopped, _))
      .Times(1);

  ExpectPolicySetup();
  ExpectStartSession(email, job);
  MockUtils();

  ScopedError error;
  manager_->StartSession(email, nothing, &out, &Resetter(&error).lvalue());
  manager_->Run();
}

TEST_F(SessionManagerDBusTest, StartSession) {
  MockChildJob* job = CreateTrivialMockJob();

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSession(email, job);
  MockUtils();

  ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerDBusTest, StartSessionNew) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSessionUnowned(email);
  MockUtils();

  ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerDBusTest, StartSessionInvalidUser) {
  TrivialInitManager();
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

TEST_F(SessionManagerDBusTest, StartSessionDevicePolicyFailure) {
  TrivialInitManager();
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

TEST_F(SessionManagerDBusTest, StartOwnerSession) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartOwnerSession(email);
  MockUtils();

  ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerDBusTest, StartSessionRemovesMachineInfo) {
  FilePath machine_info_file(tmpdir_.path().AppendASCII("machine_info"));

  MockChildJob* job = CreateTrivialMockJob();
  manager_->test_api().set_machine_info_file(machine_info_file);

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSession(email, job);
  MockUtils();

  EXPECT_EQ(0, file_util::WriteFile(machine_info_file, NULL, 0));
  EXPECT_TRUE(file_util::PathExists(machine_info_file));

  ScopedError error;
  EXPECT_EQ(TRUE, manager_->StartSession(email,
                                         nothing,
                                         &out,
                                         &Resetter(&error).lvalue()));

  EXPECT_FALSE(file_util::PathExists(machine_info_file));
}

TEST_F(SessionManagerDBusTest, StopSession) {
  TrivialInitManager();
  gboolean out;
  gchar nothing[] = "";
  manager_->StopSession(nothing, &out, NULL);
}

TEST_F(SessionManagerDBusTest, SetOwnerKeyShouldFail) {
  TrivialInitManager();
  EXPECT_CALL(utils_,
              SendStatusSignalToChromium(
                  StrEq(chromium::kOwnerKeySetSignal), false))
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

TEST_F(SessionManagerDBusTest, StorePolicyNoSession) {
  TrivialInitManager();
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  ExpectStorePolicy(device_policy_service_,
                    fake_policy,
                    PolicyService::KEY_ROTATE |
                    PolicyService::KEY_INSTALL_NEW |
                    PolicyService::KEY_CLOBBER);
  EXPECT_EQ(TRUE, manager_->StorePolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerDBusTest, StorePolicySessionStarted) {
  TrivialInitManager();
  manager_->test_api().set_session_started(true, "user@somewhere");
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  ExpectStorePolicy(device_policy_service_,
                    fake_policy,
                    PolicyService::KEY_ROTATE);
  EXPECT_EQ(TRUE, manager_->StorePolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerDBusTest, RetrievePolicy) {
  TrivialInitManager();
  const std::string fake_policy("fake policy");
  const std::vector<uint8> policy_data(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*device_policy_service_, Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data),
                      Return(true)));
  GArray* out_blob;
  ScopedError error;
  EXPECT_EQ(TRUE, manager_->RetrievePolicy(&out_blob,
                                           &Resetter(&error).lvalue()));
  EXPECT_EQ(fake_policy.size(), out_blob->len);
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob->data));
  g_array_free(out_blob, TRUE);
}

TEST_F(SessionManagerDBusTest, StoreUserPolicyNoSession) {
  TrivialInitManager();
  MockUtils();
  EXPECT_CALL(utils_, SetAndSendGError(_, _, _));

  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  EXPECT_EQ(FALSE, manager_->StoreUserPolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerDBusTest, StoreUserPolicySessionStarted) {
  MockChildJob* job = CreateTrivialMockJob();
  MockUtils();

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSession(email, job);
  manager_->StartSession(email, nothing, &out, NULL);

  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  EXPECT_CALL(*user_policy_service_,
              Store(CastEq(fake_policy),
                    fake_policy.size(),
                    _,
                    PolicyService::KEY_ROTATE |
                    PolicyService::KEY_INSTALL_NEW))
      .WillOnce(Return(true));
  EXPECT_EQ(TRUE, manager_->StoreUserPolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerDBusTest, RetrieveUserPolicyNoSession) {
  TrivialInitManager();
  MockUtils();

  GArray* out_blob = NULL;
  ScopedError error;
  EXPECT_EQ(FALSE, manager_->RetrieveUserPolicy(&out_blob,
                                                &Resetter(&error).lvalue()));
  EXPECT_FALSE(out_blob);
}

TEST_F(SessionManagerDBusTest, RetrieveUserPolicySessionStarted) {
  MockChildJob* job = CreateTrivialMockJob();
  MockUtils();

  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSession(email, job);
  manager_->StartSession(email, nothing, &out, NULL);

  const std::string fake_policy("fake policy");
  const std::vector<uint8> policy_data(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*user_policy_service_, Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data),
                      Return(true)));
  GArray* out_blob = NULL;
  ScopedError error;
  EXPECT_EQ(TRUE, manager_->RetrieveUserPolicy(&out_blob,
                                               &Resetter(&error).lvalue()));
  EXPECT_EQ(fake_policy.size(), out_blob->len);
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob->data));
  g_array_free(out_blob, TRUE);
}

TEST_F(SessionManagerDBusTest, RestartJobUnknownPid) {
  TrivialInitManager();
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

TEST_F(SessionManagerDBusTest, RestartJob) {
  MockChildJob* job = CreateTrivialMockJob();
  ExpectChildJobClearOneTimeArgument(job);
  manager_->test_api().set_child_pid(0, kDummyPid);
  EXPECT_CALL(utils_, kill(-kDummyPid, getuid(), SIGKILL))
      .WillOnce(Return(0));

  EXPECT_CALL(*job, GetName())
      .WillRepeatedly(Return(std::string("chrome")));
  EXPECT_CALL(*job, SetArguments("dummy"))
      .Times(1);
  EXPECT_CALL(*job, RecordTime())
      .Times(1);
  std::string email_string("");
  ExpectGuestSession(email_string, job);
  MockUtils();

  MockChildProcess proc(kDummyPid, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork())
      .WillOnce(Return(proc.pid()));

  gboolean out;
  gint pid = kDummyPid;
  gchar arguments[] = "dummy";
  EXPECT_EQ(TRUE, manager_->RestartJob(pid, arguments, &out, NULL));
  EXPECT_EQ(TRUE, out);
}

TEST_F(SessionManagerDBusTest, RestartJobWrongPid) {
  TrivialInitManager();
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
