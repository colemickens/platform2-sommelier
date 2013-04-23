// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_impl.h"

#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/child_job.h"
#include "login_manager/device_management_backend.pb.h"
#include "login_manager/file_checker.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_child_process.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_process_manager_service.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/mock_upstart_signal_emitter.h"
#include "login_manager/mock_user_policy_service_factory.h"

using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::_;

using chromeos::Resetter;
using chromeos::glib::ScopedError;

using std::string;
using std::vector;

namespace login_manager {

class SessionManagerImplTest : public ::testing::Test {
 public:
  SessionManagerImplTest()
      : upstart_(new MockUpstartSignalEmitter),
        device_policy_service_(new MockDevicePolicyService),
        user_policy_service_(NULL),
        impl_(scoped_ptr<UpstartSignalEmitter>(upstart_),
              &manager_,
              &metrics_,
              &utils_) {
  }

  virtual ~SessionManagerImplTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());

    MockUserPolicyServiceFactory* factory = new MockUserPolicyServiceFactory;
    EXPECT_CALL(*factory, Create(_))
        .Times(AtMost(1))
        .WillRepeatedly(
            InvokeWithoutArgs(
                this, &SessionManagerImplTest::CreateUserPolicyService));
    scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy(
        new DeviceLocalAccountPolicyService(tmpdir_.path(), NULL, NULL));
    impl_.InjectPolicyServices(device_policy_service_,
                               scoped_ptr<UserPolicyServiceFactory>(factory),
                               device_local_account_policy.Pass());
  }

  virtual void TearDown() OVERRIDE {
  }

 protected:
  void ExpectStartSession(const std::string& email_string) {
    ExpectSessionBoilerplate(email_string, false, false);
  }

  void ExpectGuestSession() {
    ExpectSessionBoilerplate(SessionManagerImpl::kIncognitoUser, true, false);
  }

  void ExpectStartOwnerSession(const std::string& email_string) {
    ExpectSessionBoilerplate(email_string, false, true);
  }

  void ExpectStartSessionUnowned(const std::string& email_string,
                                 bool mitigating) {
    EXPECT_CALL(manager_, SetBrowserSessionForUser(email_string))
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
    EXPECT_CALL(*device_policy_service_, Mitigating())
        .WillRepeatedly(Return(mitigating));
    if (!mitigating)
      EXPECT_CALL(manager_, RunKeyGenerator()).Times(1);

    EXPECT_CALL(utils_,
                EmitSignalWithStringArgs(
                    StrEq(login_manager::kSessionStateChangedSignal),
                    ElementsAre(SessionManagerImpl::kStarted, _)))
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

  void ExpectAndRunStartSession(const char const_email[]) {
    gboolean out;
    // Need a non-const gchar*.
    gchar* email = g_strdup(const_email);
    gchar nothing[] = "";

    ExpectStartSession(email);
    EXPECT_EQ(TRUE, impl_.StartSession(email, nothing, &out, NULL));
    g_free(email);
  }

  void ExpectAndRunGuestSession() {
    gboolean out;
    // Need a non-const gchar*.
    gchar* incognito = g_strdup(SessionManagerImpl::kIncognitoUser);
    gchar nothing[] = "";

    ExpectGuestSession();
    EXPECT_EQ(TRUE, impl_.StartSession(incognito, nothing, &out, NULL));
    g_free(incognito);
  }

  PolicyService* CreateUserPolicyService() {
    user_policy_service_ = new MockPolicyService();
    return user_policy_service_;
  }

  // Caller takes ownership.
  GArray* CreateArray(const char* input, const int len) {
    GArray* output = g_array_new(FALSE, FALSE, 1);
    g_array_append_vals(output, input, len);
    return output;
  }

  // These are bare pointers, not scoped_ptrs, because we need to give them
  // to a SessionManagerImpl instance, but also be able to set expectations
  // on them after we hand them off.
  MockUpstartSignalEmitter* upstart_;
  MockDevicePolicyService* device_policy_service_;
  MockPolicyService* user_policy_service_;

  MockProcessManagerService manager_;
  MockMetrics metrics_;
  MockSystemUtils utils_;

  SessionManagerImpl impl_;
  base::ScopedTempDir tmpdir_;

  static const pid_t kDummyPid;
 private:
  void ExpectSessionBoilerplate(const std::string& email_string,
                                bool guest,
                                bool for_owner) {
    EXPECT_CALL(manager_, SetBrowserSessionForUser(email_string))
        .Times(1);
    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(for_owner),
                        Return(true)));
    // Confirm that the key is present.
    EXPECT_CALL(*device_policy_service_, KeyMissing())
        .WillOnce(Return(false));

    EXPECT_CALL(metrics_, SendLoginUserType(false, guest, for_owner))
        .Times(1);
    EXPECT_CALL(utils_,
                EmitSignalWithStringArgs(
                    StrEq(login_manager::kSessionStateChangedSignal),
                    ElementsAre(SessionManagerImpl::kStarted, _)))
        .Times(1);
    EXPECT_CALL(utils_,
                AtomicFileWrite(FilePath(SessionManagerImpl::kLoggedInFlag),
                                StrEq("1"), 1))
        .Times(1);
    EXPECT_CALL(utils_, IsDevMode())
        .WillOnce(Return(false));
  }

  DISALLOW_COPY_AND_ASSIGN(SessionManagerImplTest);
};

const pid_t SessionManagerImplTest::kDummyPid = 4;

TEST_F(SessionManagerImplTest, EmitLoginPromptVisible) {
  const char event_name[] = "login-prompt-visible";
  EXPECT_CALL(metrics_, RecordStats(StrEq(event_name))).Times(1);
  EXPECT_CALL(utils_,
              EmitSignal(StrEq(login_manager::kLoginPromptVisibleSignal)))
      .Times(1);
  EXPECT_TRUE(impl_.EmitLoginPromptVisible(NULL));
}

TEST_F(SessionManagerImplTest, EnableChromeTesting) {
  string expected_testing_path("a/temp/place");
  // DBus arrays are null-terminated.
  const gchar* args1[] = {"--repeat-arg", "--one-time-arg", NULL};
  gchar* testing_path = NULL;

  utils_.SetUniqueFilename(expected_testing_path);
  EXPECT_CALL(manager_,
              RestartBrowserWithArgs(
                  ElementsAre(args1[0], args1[1],
                              HasSubstr(expected_testing_path)),
                  true))
      .Times(1);

  EXPECT_TRUE(impl_.EnableChromeTesting(false, args1, &testing_path, NULL));
  ASSERT_TRUE(testing_path != NULL);
  EXPECT_TRUE(EndsWith(testing_path, expected_testing_path, false));

  // Calling again, without forcing relaunch, should not do anything.
  testing_path = NULL;
  EXPECT_TRUE(impl_.EnableChromeTesting(false, args1, &testing_path, NULL));
  ASSERT_TRUE(testing_path != NULL);
  EXPECT_TRUE(EndsWith(testing_path, expected_testing_path, false));

  // Force relaunch.  Should go through the whole path again.
  const gchar* args2[] = {"--dummy", "--repeat-arg", NULL};
  testing_path = NULL;
  EXPECT_CALL(manager_,
              RestartBrowserWithArgs(
                  ElementsAre(args2[0], args2[1],
                              HasSubstr(expected_testing_path)),
                  true))
      .Times(1);

  EXPECT_TRUE(impl_.EnableChromeTesting(true, args2, &testing_path, NULL));
  ASSERT_TRUE(testing_path != NULL);
  EXPECT_TRUE(EndsWith(testing_path, expected_testing_path, false));
}

TEST_F(SessionManagerImplTest, StartSession) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSession(email);

  ScopedError error;
  EXPECT_EQ(TRUE, impl_.StartSession(email,
                                     nothing,
                                     &out,
                                     &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerImplTest, StartSessionNew) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSessionUnowned(email, false);

  ScopedError error;
  EXPECT_EQ(TRUE, impl_.StartSession(email,
                                     nothing,
                                     &out,
                                     &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerImplTest, StartSessionInvalidUser) {
  gboolean out;
  gchar email[] = "user";
  gchar nothing[] = "";
  ScopedError error;
  EXPECT_EQ(FALSE, impl_.StartSession(email,
                                      nothing,
                                      &out,
                                      &Resetter(&error).lvalue()));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_INVALID_EMAIL, error->code);
}

TEST_F(SessionManagerImplTest, StartSessionDevicePolicyFailure) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ScopedError error;

  // Upon the owner login check, return an error.
  EXPECT_CALL(*device_policy_service_,
              CheckAndHandleOwnerLogin(StrEq(email), _, _))
      .WillOnce(Return(false));

  EXPECT_EQ(FALSE, impl_.StartSession(email,
                                      nothing,
                                      &out,
                                      &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerImplTest, StartOwnerSession) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartOwnerSession(email);

  ScopedError error;
  EXPECT_EQ(TRUE, impl_.StartSession(email,
                                     nothing,
                                     &out,
                                     &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerImplTest, StartSessionKeyMitigation) {
  gboolean out;
  gchar email[] = "user@somewhere";
  gchar nothing[] = "";
  ExpectStartSessionUnowned(email, true);

  ScopedError error;
  EXPECT_EQ(TRUE, impl_.StartSession(email,
                                     nothing,
                                     &out,
                                     &Resetter(&error).lvalue()));
}

TEST_F(SessionManagerImplTest, StopSession) {
  gboolean out;
  gchar nothing[] = "";
  EXPECT_CALL(manager_, ScheduleShutdown()).Times(1);
  impl_.StopSession(nothing, &out, NULL);
}

TEST_F(SessionManagerImplTest, StorePolicyNoSession) {
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  ExpectStorePolicy(device_policy_service_,
                    fake_policy,
                    PolicyService::KEY_ROTATE |
                    PolicyService::KEY_INSTALL_NEW |
                    PolicyService::KEY_CLOBBER);
  EXPECT_EQ(TRUE, impl_.StorePolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerImplTest, StorePolicySessionStarted) {
  ExpectAndRunStartSession("user@somewhere");
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  ExpectStorePolicy(device_policy_service_,
                    fake_policy,
                    PolicyService::KEY_ROTATE);
  EXPECT_EQ(TRUE, impl_.StorePolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerImplTest, RetrievePolicy) {
  const std::string fake_policy("fake policy");
  const std::vector<uint8> policy_data(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*device_policy_service_, Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data),
                      Return(true)));
  GArray* out_blob;
  ScopedError error;
  EXPECT_EQ(TRUE, impl_.RetrievePolicy(&out_blob,
                                       &Resetter(&error).lvalue()));
  EXPECT_EQ(fake_policy.size(), out_blob->len);
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob->data));
  g_array_free(out_blob, TRUE);
}

TEST_F(SessionManagerImplTest, StoreUserPolicyNoSession) {
  EXPECT_CALL(utils_, SetAndSendGError(_, _, _));

  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  EXPECT_EQ(FALSE, impl_.StoreUserPolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerImplTest, StoreUserPolicySessionStarted) {
  ExpectAndRunStartSession("user@somewhere.com");
  const std::string fake_policy("fake policy");
  GArray* policy_blob = CreateArray(fake_policy.c_str(), fake_policy.size());
  EXPECT_CALL(*user_policy_service_,
              Store(CastEq(fake_policy),
                    fake_policy.size(),
                    _,
                    PolicyService::KEY_ROTATE |
                    PolicyService::KEY_INSTALL_NEW))
      .WillOnce(Return(true));
  EXPECT_EQ(TRUE, impl_.StoreUserPolicy(policy_blob, NULL));
  g_array_free(policy_blob, TRUE);
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicyNoSession) {
  GArray* out_blob = NULL;
  ScopedError error;
  EXPECT_EQ(FALSE, impl_.RetrieveUserPolicy(&out_blob,
                                            &Resetter(&error).lvalue()));
  EXPECT_FALSE(out_blob);
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicySessionStarted) {
  ExpectAndRunStartSession("user@somewhere");
  const std::string fake_policy("fake policy");
  const std::vector<uint8> policy_data(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*user_policy_service_, Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data),
                      Return(true)));
  GArray* out_blob = NULL;
  ScopedError error;
  EXPECT_EQ(TRUE, impl_.RetrieveUserPolicy(&out_blob,
                                           &Resetter(&error).lvalue()));
  EXPECT_EQ(fake_policy.size(), out_blob->len);
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob->data));
  g_array_free(out_blob, TRUE);
}

TEST_F(SessionManagerImplTest, RestartJobUnknownPid) {
  gboolean out;
  gint pid = kDummyPid;
  gchar arguments[] = "";
  ScopedError error;
  EXPECT_CALL(manager_, IsBrowser(pid)).WillOnce(Return(false));

  EXPECT_EQ(FALSE, impl_.RestartJob(pid, arguments, &out,
                                    &Resetter(&error).lvalue()));
  EXPECT_EQ(CHROMEOS_LOGIN_ERROR_UNKNOWN_PID, error->code);
  EXPECT_EQ(FALSE, out);
}

TEST_F(SessionManagerImplTest, RestartJob) {
  gboolean out;
  gint pid = kDummyPid;
  gchar arguments[] = "dummy";

  EXPECT_CALL(manager_, IsBrowser(pid)).WillOnce(Return(true));
  EXPECT_CALL(manager_, RestartBrowserWithArgs(ElementsAre(arguments), false))
      .Times(1);
  ExpectGuestSession();

  EXPECT_EQ(TRUE, impl_.RestartJob(pid, arguments, &out, NULL));
  EXPECT_EQ(TRUE, out);
}

TEST_F(SessionManagerImplTest, RestartJobWithAuthBadCookie) {
  gboolean out;
  gint pid = kDummyPid;
  gchar cookie[] = "bogus-cookie";
  gchar arguments[] = "dummy";

  // Ensure there's no browser restarting.
  EXPECT_CALL(manager_, RestartBrowserWithArgs(_, _)).Times(0);
  EXPECT_EQ(FALSE, impl_.RestartJobWithAuth(pid, cookie, arguments, &out,
                                            NULL));
  EXPECT_EQ(FALSE, out);
}

TEST_F(SessionManagerImplTest, LockScreen) {
  ExpectAndRunStartSession("user@somewhere");
  EXPECT_CALL(utils_, EmitSignal(StrEq(chromium::kLockScreenSignal))).Times(1);
  GError *error = NULL;
  EXPECT_EQ(TRUE, impl_.LockScreen(&error));
  EXPECT_EQ(TRUE, impl_.ScreenIsLocked());
}

TEST_F(SessionManagerImplTest, LockScreenNoSession) {
  EXPECT_CALL(utils_, EmitSignal(StrEq(chromium::kLockScreenSignal))).Times(0);
  GError *error = NULL;
  EXPECT_EQ(FALSE, impl_.LockScreen(&error));
}

TEST_F(SessionManagerImplTest, LockScreenGuest) {
  ExpectAndRunGuestSession();
  EXPECT_CALL(utils_, EmitSignal(StrEq(chromium::kLockScreenSignal))).Times(0);
  GError *error = NULL;
  EXPECT_EQ(FALSE, impl_.LockScreen(&error));
}

TEST_F(SessionManagerImplTest, LockUnlockScreen) {
  ExpectAndRunStartSession("user@somewhere");
  EXPECT_CALL(utils_, EmitSignal(StrEq(chromium::kLockScreenSignal))).Times(1);
  GError *error = NULL;
  EXPECT_EQ(TRUE, impl_.LockScreen(&error));
  EXPECT_EQ(TRUE, impl_.ScreenIsLocked());

  EXPECT_CALL(utils_, EmitSignal(StrEq(login_manager::kScreenIsLockedSignal)))
      .Times(1);
  EXPECT_EQ(TRUE, impl_.HandleLockScreenShown(&error));

  EXPECT_CALL(utils_, EmitSignal(StrEq(chromium::kUnlockScreenSignal)))
      .Times(1);
  EXPECT_EQ(TRUE, impl_.UnlockScreen(&error));
  EXPECT_EQ(TRUE, impl_.ScreenIsLocked());

  EXPECT_CALL(utils_, EmitSignal(StrEq(login_manager::kScreenIsUnlockedSignal)))
      .Times(1);
  EXPECT_EQ(TRUE, impl_.HandleLockScreenDismissed(&error));
  EXPECT_EQ(FALSE, impl_.ScreenIsLocked());
}

TEST_F(SessionManagerImplTest, StartDeviceWipeAlreadyLoggedIn) {
  FilePath logged_in_path(SessionManagerImpl::kLoggedInFlag);
  EXPECT_CALL(utils_, Exists(logged_in_path)).WillOnce(Return(true));
  GError *error = NULL;
  gboolean done = FALSE;
  EXPECT_EQ(FALSE, impl_.StartDeviceWipe(&done, &error));
}

TEST_F(SessionManagerImplTest, StartDeviceWipe) {
  FilePath logged_in_path(SessionManagerImpl::kLoggedInFlag);
  FilePath reset_path(SessionManagerImpl::kResetFile);
  EXPECT_CALL(utils_, Exists(logged_in_path)).WillOnce(Return(false));
  EXPECT_CALL(utils_, AtomicFileWrite(reset_path, _, _)).WillOnce(Return(true));
  EXPECT_CALL(utils_, CallMethodOnPowerManager(_)).Times(1);
  gboolean done = FALSE;
  EXPECT_EQ(TRUE, impl_.StartDeviceWipe(&done, NULL));
  EXPECT_TRUE(done);
}

TEST_F(SessionManagerImplTest, ImportValidateAndStoreGeneratedKey) {
  base::ScopedTempDir tmpdir;
  FilePath key_file_path;
  string key("key_contents");
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir.path(),
                                                  &key_file_path));
  ASSERT_EQ(file_util::WriteFile(key_file_path, key.c_str(), key.size()),
            key.size());
  EXPECT_CALL(*device_policy_service_,
              ValidateAndStoreOwnerKey(_, StrEq(key)))
      .WillOnce(Return(true));

  impl_.ImportValidateAndStoreGeneratedKey(key_file_path);
  EXPECT_FALSE(file_util::PathExists(key_file_path));
}

class SessionManagerImplStaticTest : public ::testing::Test {
 public:
  SessionManagerImplStaticTest() {}
  virtual ~SessionManagerImplStaticTest() {}

  bool ValidateEmail(const std::string& email_address) {
    return SessionManagerImpl::ValidateEmail(email_address);
  }
 };

TEST_F(SessionManagerImplStaticTest, EmailAddressTest) {
  const char valid[] = "user_who+we.like@some-where.com";
  EXPECT_TRUE(ValidateEmail(valid));
}

TEST_F(SessionManagerImplStaticTest, EmailAddressNonAsciiTest) {
  char invalid[4] = "a@m";
  invalid[2] = 254;
  EXPECT_FALSE(ValidateEmail(invalid));
}

TEST_F(SessionManagerImplStaticTest, EmailAddressNoAtTest) {
  const char no_at[] = "user";
  EXPECT_FALSE(ValidateEmail(no_at));
}

TEST_F(SessionManagerImplStaticTest, EmailAddressTooMuchAtTest) {
  const char extra_at[] = "user@what@where";
  EXPECT_FALSE(ValidateEmail(extra_at));
}

}  // namespace login_manager
