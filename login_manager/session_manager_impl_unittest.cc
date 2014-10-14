// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_impl.h"

#include <stdint.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/strings/string_util.h>
#include <chromeos/cryptohome.h>
#include <chromeos/dbus/service_constants.h>
#include <crypto/scoped_nss_types.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bindings/device_management_backend.pb.h"
#include "login_manager/dbus_error_types.h"
#include "login_manager/device_local_account_policy_service.h"
#include "login_manager/file_checker.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_dbus_signal_emitter.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_nss_util.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_process_manager_service.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/mock_user_policy_service_factory.h"
#include "login_manager/server_backed_state_key_generator.h"
#include "login_manager/stub_upstart_signal_emitter.h"

using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::_;

using chromeos::cryptohome::home::SanitizeUserName;
using chromeos::cryptohome::home::SetSystemSalt;
using chromeos::cryptohome::home::kGuestUserName;

using std::map;
using std::string;
using std::vector;

namespace login_manager {

class SessionManagerImplTest : public ::testing::Test {
 public:
  SessionManagerImplTest()
      : device_policy_service_(new MockDevicePolicyService),
        state_key_generator_(&utils_),
        impl_(scoped_ptr<UpstartSignalEmitter>(new StubUpstartSignalEmitter),
              &dbus_emitter_,
              base::Bind(&SessionManagerImplTest::FakeLockScreen,
                         base::Unretained(this)),
              base::Bind(&SessionManagerImplTest::FakeRestartDevice,
                         base::Unretained(this)),
              &key_gen_,
              &state_key_generator_,
              &manager_,
              &metrics_,
              &nss_,
              &utils_),
        fake_salt_("fake salt"),
        actual_locks_(0),
        expected_locks_(0),
        actual_restarts_(0),
        expected_restarts_(0) {}

  virtual ~SessionManagerImplTest() {}

  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    SetSystemSalt(&fake_salt_);

    MockUserPolicyServiceFactory* factory = new MockUserPolicyServiceFactory;
    EXPECT_CALL(*factory, Create(_)).WillRepeatedly(
        Invoke(this, &SessionManagerImplTest::CreateUserPolicyService));
    scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy(
        new DeviceLocalAccountPolicyService(tmpdir_.path(), NULL, NULL));
    impl_.InjectPolicyServices(
        scoped_ptr<DevicePolicyService>(device_policy_service_),
        scoped_ptr<UserPolicyServiceFactory>(factory),
        device_local_account_policy.Pass());
  }

  void TearDown() override {
    SetSystemSalt(NULL);
    EXPECT_EQ(actual_locks_, expected_locks_);
    EXPECT_EQ(actual_restarts_, expected_restarts_);
  }

 protected:
  void ExpectStartSession(const string& email_string) {
    ExpectSessionBoilerplate(email_string, false, false);
  }

  void ExpectGuestSession() {
    ExpectSessionBoilerplate(kGuestUserName, true, false);
  }

  void ExpectStartOwnerSession(const string& email_string) {
    ExpectSessionBoilerplate(email_string, false, true);
  }

  void ExpectStartSessionUnowned(const string& email_string) {
    ExpectStartSessionUnownedBoilerplate(email_string, false, false);
  }

  void ExpectStartSessionOwningInProcess(const string& email_string) {
    ExpectStartSessionUnownedBoilerplate(email_string, false, true);
  }

  void ExpectStartSessionOwnerLost(const string& email_string) {
    ExpectStartSessionUnownedBoilerplate(email_string, true, false);
  }

  void ExpectLockScreen() { expected_locks_ = 1; }

  void ExpectDeviceRestart() { expected_restarts_ = 1; }

  void ExpectStorePolicy(MockDevicePolicyService* service,
                         const string& policy,
                         int flags) {
    EXPECT_CALL(*service, Store(CastEq(policy), policy.size(), _, flags))
        .WillOnce(Return(true));
  }

  void ExpectAndRunStartSession(const string& email) {
    ExpectStartSession(email);
    EXPECT_TRUE(impl_.StartSession(email, kNothing, NULL));
  }

  void ExpectAndRunGuestSession() {
    ExpectGuestSession();
    EXPECT_TRUE(impl_.StartSession(kGuestUserName, kNothing, NULL));
  }

  PolicyService* CreateUserPolicyService(const string& username) {
    user_policy_services_[username] = new MockPolicyService();
    return user_policy_services_[username];
  }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(device_policy_service_);
    for (map<string, MockPolicyService*>::iterator it =
             user_policy_services_.begin();
         it != user_policy_services_.end();
         ++it) {
      Mock::VerifyAndClearExpectations(it->second);
    }
    Mock::VerifyAndClearExpectations(&manager_);
    Mock::VerifyAndClearExpectations(&metrics_);
    Mock::VerifyAndClearExpectations(&nss_);
    Mock::VerifyAndClearExpectations(&utils_);
  }

  // These are bare pointers, not scoped_ptrs, because we need to give them
  // to a SessionManagerImpl instance, but also be able to set expectations
  // on them after we hand them off.
  MockDevicePolicyService* device_policy_service_;
  map<string, MockPolicyService*> user_policy_services_;

  MockDBusSignalEmitter dbus_emitter_;
  MockKeyGenerator key_gen_;
  ServerBackedStateKeyGenerator state_key_generator_;
  MockProcessManagerService manager_;
  MockMetrics metrics_;
  MockNssUtil nss_;
  MockSystemUtils utils_;

  SessionManagerImpl impl_;
  SessionManagerImpl::Error error_;
  base::ScopedTempDir tmpdir_;

  static const pid_t kDummyPid;
  static const char kNothing[];
  static const char kSaneEmail[];

 private:
  void ExpectSessionBoilerplate(const string& email_string,
                                bool guest,
                                bool for_owner) {
    EXPECT_CALL(manager_,
                SetBrowserSessionForUser(StrEq(email_string),
                                         StrEq(SanitizeUserName(email_string))))
        .Times(1);
    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _, _))
        .WillOnce(DoAll(SetArgumentPointee<2>(for_owner), Return(true)));
    // Confirm that the key is present.
    EXPECT_CALL(*device_policy_service_, KeyMissing()).WillOnce(Return(false));

    EXPECT_CALL(metrics_, SendLoginUserType(false, guest, for_owner)).Times(1);
    EXPECT_CALL(
        dbus_emitter_,
        EmitSignalWithString(StrEq(login_manager::kSessionStateChangedSignal),
                             StrEq(SessionManagerImpl::kStarted))).Times(1);
    EXPECT_CALL(utils_, IsDevMode()).WillOnce(Return(false));
  }

  void ExpectStartSessionUnownedBoilerplate(const string& email_string,
                                            bool mitigating,
                                            bool owning_in_progress) {
    EXPECT_CALL(manager_,
                SetBrowserSessionForUser(StrEq(email_string),
                                         StrEq(SanitizeUserName(email_string))))
        .Times(1);

    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(email_string), _, _, _))
        .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)));

    // Indicate that there is no owner key in order to trigger a new one to be
    // generated.
    EXPECT_CALL(*device_policy_service_, KeyMissing()).WillOnce(Return(true));
    EXPECT_CALL(*device_policy_service_, Mitigating())
        .WillRepeatedly(Return(mitigating));
    if (!mitigating && !owning_in_progress)
      EXPECT_CALL(key_gen_, Start(StrEq(email_string))).Times(1);
    else
      EXPECT_CALL(key_gen_, Start(_)).Times(0);

    EXPECT_CALL(
        dbus_emitter_,
        EmitSignalWithString(StrEq(login_manager::kSessionStateChangedSignal),
                             StrEq(SessionManagerImpl::kStarted))).Times(1);
    EXPECT_CALL(utils_, IsDevMode()).WillOnce(Return(false));
  }

  void FakeLockScreen() { actual_locks_++; }

  void FakeRestartDevice() { actual_restarts_++; }

  string fake_salt_;

  // Used by fake closures that simulate calling chrome and powerd to lock
  // the screen and restart the device.
  uint32_t actual_locks_;
  uint32_t expected_locks_;
  uint32_t actual_restarts_;
  uint32_t expected_restarts_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerImplTest);
};

const pid_t SessionManagerImplTest::kDummyPid = 4;
const char SessionManagerImplTest::kNothing[] = "";
const char SessionManagerImplTest::kSaneEmail[] = "user@somewhere.com";

TEST_F(SessionManagerImplTest, EmitLoginPromptVisible) {
  const char event_name[] = "login-prompt-visible";
  EXPECT_CALL(metrics_, RecordStats(StrEq(event_name))).Times(1);
  EXPECT_CALL(dbus_emitter_,
              EmitSignal(StrEq(login_manager::kLoginPromptVisibleSignal)))
      .Times(1);
  impl_.EmitLoginPromptVisible(&error_);
}

TEST_F(SessionManagerImplTest, EnableChromeTesting) {
  std::vector<std::string> args;
  args.push_back("--repeat-arg");
  args.push_back("--one-time-arg");

  base::FilePath expected = utils_.GetUniqueFilename();
  ASSERT_FALSE(expected.empty());
  string expected_testing_path = expected.value();

  EXPECT_CALL(
      manager_,
      RestartBrowserWithArgs(
          ElementsAre(args[0], args[1], HasSubstr(expected_testing_path)),
          true)).Times(1);

  string testing_path = impl_.EnableChromeTesting(false, args, NULL);
  EXPECT_TRUE(EndsWith(testing_path, expected_testing_path, false));

  // Calling again, without forcing relaunch, should not do anything.
  testing_path.clear();
  testing_path = impl_.EnableChromeTesting(false, args, NULL);
  EXPECT_TRUE(EndsWith(testing_path, expected_testing_path, false));

  // Force relaunch.  Should go through the whole path again.
  args[0] = "--dummy";
  args[1] = "--repeat-arg";
  testing_path.empty();
  EXPECT_CALL(
      manager_,
      RestartBrowserWithArgs(
          ElementsAre(args[0], args[1], HasSubstr(expected_testing_path)),
          true)).Times(1);

  testing_path = impl_.EnableChromeTesting(true, args, NULL);
  EXPECT_TRUE(EndsWith(testing_path, expected_testing_path, false));
}

TEST_F(SessionManagerImplTest, StartSession) {
  ExpectStartSession(kSaneEmail);
  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, &error_));
}

TEST_F(SessionManagerImplTest, StartSession_New) {
  ExpectStartSessionUnowned(kSaneEmail);
  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, &error_));
}

TEST_F(SessionManagerImplTest, StartSession_InvalidUser) {
  const char bad_email[] = "user";
  EXPECT_FALSE(impl_.StartSession(bad_email, kNothing, &error_));
  EXPECT_EQ(dbus_error::kInvalidAccount, error_.name());
}

TEST_F(SessionManagerImplTest, StartSession_Twice) {
  ExpectStartSession(kSaneEmail);
  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));

  EXPECT_FALSE(impl_.StartSession(kSaneEmail, kNothing, &error_));
  EXPECT_EQ(dbus_error::kSessionExists, error_.name());
}

TEST_F(SessionManagerImplTest, StartSession_TwoUsers) {
  ExpectStartSession(kSaneEmail);
  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));
  VerifyAndClearExpectations();

  const char email2[] = "user2@somewhere";
  ExpectStartSession(email2);
  EXPECT_TRUE(impl_.StartSession(email2, kNothing, NULL));
}

TEST_F(SessionManagerImplTest, StartSession_OwnerAndOther) {
  ExpectStartSessionUnowned(kSaneEmail);

  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));
  VerifyAndClearExpectations();

  const char email2[] = "user2@somewhere";
  ExpectStartSession(email2);
  EXPECT_TRUE(impl_.StartSession(email2, kNothing, NULL));
}

TEST_F(SessionManagerImplTest, StartSession_OwnerRace) {
  ExpectStartSessionUnowned(kSaneEmail);

  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));
  VerifyAndClearExpectations();

  const char email2[] = "user2@somewhere";
  ExpectStartSessionOwningInProcess(email2);
  EXPECT_TRUE(impl_.StartSession(email2, kNothing, NULL));
}

TEST_F(SessionManagerImplTest, StartSession_BadNssDB) {
  nss_.MakeBadDB();
  EXPECT_FALSE(impl_.StartSession(kSaneEmail, kNothing, &error_));
  EXPECT_EQ(dbus_error::kNoUserNssDb, error_.name());
}

TEST_F(SessionManagerImplTest, StartSession_DevicePolicyFailure) {
  // Upon the owner login check, return an error.
  EXPECT_CALL(*device_policy_service_,
              CheckAndHandleOwnerLogin(StrEq(kSaneEmail), _, _, _))
      .WillOnce(Return(false));

  EXPECT_FALSE(impl_.StartSession(kSaneEmail, kNothing, &error_));
}

TEST_F(SessionManagerImplTest, StartSession_Owner) {
  ExpectStartOwnerSession(kSaneEmail);
  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));
}

TEST_F(SessionManagerImplTest, StartSession_KeyMitigation) {
  ExpectStartSessionOwnerLost(kSaneEmail);
  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));
}

TEST_F(SessionManagerImplTest, StopSession) {
  EXPECT_CALL(manager_, ScheduleShutdown()).Times(1);
  EXPECT_TRUE(impl_.StopSession());
}

TEST_F(SessionManagerImplTest, StorePolicy_NoSession) {
  const string fake_policy("fake policy");
  const vector<uint8_t> policy_blob(fake_policy.begin(), fake_policy.end());
  ExpectStorePolicy(device_policy_service_,
                    fake_policy,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW |
                        PolicyService::KEY_CLOBBER);
  impl_.StorePolicy(policy_blob.data(), policy_blob.size(), NULL);
}

TEST_F(SessionManagerImplTest, StorePolicy_SessionStarted) {
  ExpectAndRunStartSession(kSaneEmail);
  const string fake_policy("fake policy");
  const vector<uint8_t> policy_blob(fake_policy.begin(), fake_policy.end());
  ExpectStorePolicy(
      device_policy_service_, fake_policy, PolicyService::KEY_ROTATE);
  impl_.StorePolicy(policy_blob.data(), policy_blob.size(), NULL);
}

TEST_F(SessionManagerImplTest, RetrievePolicy) {
  const string fake_policy("fake policy");
  const vector<uint8_t> policy_data(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*device_policy_service_, Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data), Return(true)));
  vector<uint8_t> out_blob;
  impl_.RetrievePolicy(&out_blob, NULL);

  EXPECT_EQ(fake_policy.size(), out_blob.size());
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob.begin()));
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_NoSession) {
  const string fake_policy("fake policy");
  const vector<uint8_t> policy_blob(fake_policy.begin(), fake_policy.end());
  MockPolicyServiceCompletion completion;
  EXPECT_CALL(completion,
              ReportFailure(PolicyErrorEq(dbus_error::kSessionDoesNotExist)))
      .Times(1);
  impl_.StorePolicyForUser(
      kSaneEmail, policy_blob.data(), policy_blob.size(), &completion);
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_SessionStarted) {
  ExpectAndRunStartSession(kSaneEmail);
  const string fake_policy("fake policy");
  const vector<uint8_t> policy_blob(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*user_policy_services_[kSaneEmail],
              Store(CastEq(fake_policy),
                    fake_policy.size(),
                    _,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW))
      .WillOnce(Return(true));
  impl_.StorePolicyForUser(
      kSaneEmail, policy_blob.data(), policy_blob.size(), NULL);
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_SecondSession) {
  ExpectAndRunStartSession(kSaneEmail);
  ASSERT_TRUE(user_policy_services_[kSaneEmail]);

  // Store policy for the signed-in user.
  const std::string fake_policy("fake policy");
  const vector<uint8_t> policy_blob(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*user_policy_services_[kSaneEmail],
              Store(CastEq(fake_policy),
                    fake_policy.size(),
                    _,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW))
      .WillOnce(Return(true));
  impl_.StorePolicyForUser(
      kSaneEmail, policy_blob.data(), policy_blob.size(), NULL);
  Mock::VerifyAndClearExpectations(user_policy_services_[kSaneEmail]);

  // Storing policy for another username fails before his session starts.
  const char user2[] = "user2@somewhere.com";
  MockPolicyServiceCompletion completion;
  EXPECT_CALL(completion,
              ReportFailure(PolicyErrorEq(dbus_error::kSessionDoesNotExist)))
      .Times(1);
  impl_.StorePolicyForUser(
      user2, policy_blob.data(), policy_blob.size(), &completion);

  // Now start another session for the 2nd user.
  ExpectAndRunStartSession(user2);
  ASSERT_TRUE(user_policy_services_[user2]);

  // Storing policy for that user now succeeds.
  EXPECT_CALL(*user_policy_services_[user2],
              Store(CastEq(fake_policy),
                    fake_policy.size(),
                    _,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW))
      .WillOnce(Return(true));
  impl_.StorePolicyForUser(user2, policy_blob.data(), policy_blob.size(), NULL);
  Mock::VerifyAndClearExpectations(user_policy_services_[user2]);
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicy_NoSession) {
  vector<uint8_t> out_blob;
  impl_.RetrievePolicyForUser(kSaneEmail, &out_blob, &error_);
  EXPECT_EQ(out_blob.size(), 0);
  EXPECT_EQ(dbus_error::kSessionDoesNotExist, error_.name());
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicy_SessionStarted) {
  ExpectAndRunStartSession(kSaneEmail);
  const string fake_policy("fake policy");
  const vector<uint8_t> policy_data(fake_policy.begin(), fake_policy.end());
  EXPECT_CALL(*user_policy_services_[kSaneEmail], Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data), Return(true)));
  vector<uint8_t> out_blob;
  impl_.RetrievePolicyForUser(kSaneEmail, &out_blob, &error_);
  EXPECT_EQ(fake_policy.size(), out_blob.size());
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob.begin()));
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicy_SecondSession) {
  ExpectAndRunStartSession(kSaneEmail);
  ASSERT_TRUE(user_policy_services_[kSaneEmail]);

  // Retrieve policy for the signed-in user.
  const std::string fake_policy("fake policy");
  const std::vector<uint8_t> policy_data(fake_policy.begin(),
                                         fake_policy.end());
  EXPECT_CALL(*user_policy_services_[kSaneEmail], Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data), Return(true)));
  std::vector<uint8_t> out_blob;
  impl_.RetrievePolicyForUser(kSaneEmail, &out_blob, NULL);
  Mock::VerifyAndClearExpectations(user_policy_services_[kSaneEmail]);
  EXPECT_EQ(fake_policy.size(), out_blob.size());
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob.begin()));

  // Retrieving policy for another username fails before his session starts.
  const char user2[] = "user2@somewhere.com";
  out_blob.clear();
  impl_.RetrievePolicyForUser(user2, &out_blob, &error_);
  EXPECT_EQ(error_.name(), dbus_error::kSessionDoesNotExist);

  // Now start another session for the 2nd user.
  ExpectAndRunStartSession(user2);
  ASSERT_TRUE(user_policy_services_[user2]);

  // Retrieving policy for that user now succeeds.
  EXPECT_CALL(*user_policy_services_[user2], Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_data), Return(true)));
  out_blob.clear();
  impl_.RetrievePolicyForUser(user2, &out_blob, NULL);
  Mock::VerifyAndClearExpectations(user_policy_services_[user2]);
  EXPECT_EQ(fake_policy.size(), out_blob.size());
  EXPECT_TRUE(
      std::equal(fake_policy.begin(), fake_policy.end(), out_blob.begin()));
}

TEST_F(SessionManagerImplTest, RetrieveActiveSessions) {
  ExpectStartSession(kSaneEmail);
  EXPECT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));
  std::map<std::string, std::string> active_users;
  impl_.RetrieveActiveSessions(&active_users);
  EXPECT_EQ(active_users.size(), 1);
  EXPECT_EQ(active_users[kSaneEmail], SanitizeUserName(kSaneEmail));
  VerifyAndClearExpectations();

  const char email2[] = "user2@somewhere";
  ExpectStartSession(email2);
  EXPECT_TRUE(impl_.StartSession(email2, kNothing, NULL));
  active_users.clear();
  impl_.RetrieveActiveSessions(&active_users);
  EXPECT_EQ(active_users.size(), 2);
  EXPECT_EQ(active_users[kSaneEmail], SanitizeUserName(kSaneEmail));
  EXPECT_EQ(active_users[email2], SanitizeUserName(email2));
}

TEST_F(SessionManagerImplTest, RestartJob_UnknownPid) {
  EXPECT_CALL(manager_, IsBrowser(kDummyPid)).WillOnce(Return(false));

  EXPECT_FALSE(impl_.RestartJob(kDummyPid, "", &error_));
  EXPECT_EQ(dbus_error::kUnknownPid, error_.name());
}

TEST_F(SessionManagerImplTest, RestartJob) {
  const char arguments[] = "dummy";

  EXPECT_CALL(manager_, IsBrowser(kDummyPid)).WillOnce(Return(true));
  EXPECT_CALL(manager_, RestartBrowserWithArgs(ElementsAre(arguments), false))
      .Times(1);
  ExpectGuestSession();

  EXPECT_EQ(TRUE, impl_.RestartJob(kDummyPid, arguments, NULL));
}

TEST_F(SessionManagerImplTest, SupervisedUserCreation) {
  impl_.HandleSupervisedUserCreationStarting();
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.HandleSupervisedUserCreationFinished();
  EXPECT_FALSE(impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockScreen) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectLockScreen();
  impl_.LockScreen(NULL);
  EXPECT_TRUE(impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockScreen_DuringSupervisedUserCreation) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectLockScreen();
  EXPECT_CALL(dbus_emitter_, EmitSignal(_)).Times(AnyNumber());

  impl_.HandleSupervisedUserCreationStarting();
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.LockScreen(NULL);
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.HandleLockScreenShown();
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.HandleLockScreenDismissed();
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.HandleSupervisedUserCreationFinished();
  EXPECT_FALSE(impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockScreen_InterleavedSupervisedUserCreation) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectLockScreen();
  EXPECT_CALL(dbus_emitter_, EmitSignal(_)).Times(AnyNumber());

  impl_.HandleSupervisedUserCreationStarting();
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.LockScreen(NULL);
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.HandleLockScreenShown();
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.HandleSupervisedUserCreationFinished();
  EXPECT_TRUE(impl_.ShouldEndSession());
  impl_.HandleLockScreenDismissed();
  EXPECT_FALSE(impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockScreen_MultiSession) {
  ExpectAndRunStartSession("user@somewhere");
  ExpectAndRunStartSession("user2@somewhere");
  ExpectLockScreen();
  impl_.LockScreen(NULL);
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockScreen_NoSession) {
  impl_.LockScreen(&error_);
  EXPECT_EQ(dbus_error::kSessionDoesNotExist, error_.name());
}

TEST_F(SessionManagerImplTest, LockScreen_Guest) {
  ExpectAndRunGuestSession();
  impl_.LockScreen(&error_);
  EXPECT_EQ(dbus_error::kSessionExists, error_.name());
}

TEST_F(SessionManagerImplTest, LockScreen_UserAndGuest) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectAndRunGuestSession();
  ExpectLockScreen();
  impl_.LockScreen(&error_);
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockUnlockScreen) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectLockScreen();
  impl_.LockScreen(&error_);
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());

  EXPECT_CALL(dbus_emitter_,
              EmitSignal(StrEq(login_manager::kScreenIsLockedSignal))).Times(1);
  impl_.HandleLockScreenShown();
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());

  EXPECT_CALL(dbus_emitter_,
              EmitSignal(StrEq(login_manager::kScreenIsUnlockedSignal)))
      .Times(1);
  impl_.HandleLockScreenDismissed();
  EXPECT_EQ(FALSE, impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, StartDeviceWipe_AlreadyLoggedIn) {
  base::FilePath logged_in_path(SessionManagerImpl::kLoggedInFlag);
  ASSERT_FALSE(utils_.Exists(logged_in_path));
  ASSERT_TRUE(utils_.AtomicFileWrite(logged_in_path, "1"));
  impl_.StartDeviceWipe(&error_);
  EXPECT_EQ(error_.name(), dbus_error::kSessionExists);
}

TEST_F(SessionManagerImplTest, StartDeviceWipe) {
  base::FilePath logged_in_path(SessionManagerImpl::kLoggedInFlag);
  base::FilePath reset_path(SessionManagerImpl::kResetFile);
  ASSERT_TRUE(utils_.RemoveFile(logged_in_path));
  ExpectDeviceRestart();
  impl_.StartDeviceWipe(NULL);
}

TEST_F(SessionManagerImplTest, ImportValidateAndStoreGeneratedKey) {
  base::FilePath key_file_path;
  string key("key_contents");
  ASSERT_TRUE(base::CreateTemporaryFileInDir(tmpdir_.path(), &key_file_path));
  ASSERT_EQ(base::WriteFile(key_file_path, key.c_str(), key.size()),
            key.size());

  // Start a session, to set up NSSDB for the user.
  ExpectStartOwnerSession(kSaneEmail);
  ASSERT_TRUE(impl_.StartSession(kSaneEmail, kNothing, NULL));

  EXPECT_CALL(
      *device_policy_service_,
      ValidateAndStoreOwnerKey(StrEq(kSaneEmail), StrEq(key), nss_.GetSlot()))
      .WillOnce(Return(true));

  impl_.OnKeyGenerated(kSaneEmail, key_file_path);
  EXPECT_FALSE(base::PathExists(key_file_path));
}

class SessionManagerImplStaticTest : public ::testing::Test {
 public:
  SessionManagerImplStaticTest() {}
  virtual ~SessionManagerImplStaticTest() {}

  bool ValidateEmail(const string& email_address) {
    return SessionManagerImpl::ValidateEmail(email_address);
  }
};

TEST_F(SessionManagerImplStaticTest, EmailAddressTest) {
  EXPECT_TRUE(ValidateEmail("user_who+we.like@some-where.com"));
  EXPECT_TRUE(ValidateEmail("john_doe's_mail@some-where.com"));
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
