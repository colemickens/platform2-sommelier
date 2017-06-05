// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_impl.h"

#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/ptr_util.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/string_util.h>
#include <brillo/cryptohome.h>
#include <brillo/errors/error.h>
#include <chromeos/dbus/service_constants.h>
#include <crypto/scoped_nss_types.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bindings/chrome_device_policy.pb.h"
#include "bindings/device_management_backend.pb.h"
#include "login_manager/blob_util.h"
#include "login_manager/dbus_util.h"
#include "login_manager/device_local_account_policy_service.h"
#include "login_manager/fake_container_manager.h"
#include "login_manager/fake_crossystem.h"
#include "login_manager/file_checker.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_dbus_signal_emitter.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_install_attributes_reader.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_nss_util.h"
#include "login_manager/mock_object_proxy.h"
#include "login_manager/mock_policy_key.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_process_manager_service.h"
#include "login_manager/mock_server_backed_state_key_generator.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/mock_user_policy_service_factory.h"
#include "login_manager/mock_vpd_process.h"
#include "login_manager/stub_upstart_signal_emitter.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StartsWith;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::_;

using brillo::cryptohome::home::GetRootPath;
using brillo::cryptohome::home::SanitizeUserName;
using brillo::cryptohome::home::SetSystemSalt;
using brillo::cryptohome::home::kGuestUserName;

using enterprise_management::ChromeDeviceSettingsProto;

using std::map;
using std::string;
using std::vector;

namespace login_manager {

namespace {

constexpr pid_t kAndroidPid = 10;

enum class DataDirType {
  DATA_DIR_AVAILABLE = 0,
  DATA_DIR_MISSING = 1,
};

enum class OldDataDirType {
  OLD_DATA_DIR_NOT_EMPTY = 0,
  OLD_DATA_DIR_EMPTY = 1,
  OLD_DATA_FILE_EXISTS = 2,
};

}  // anonymous namespace

class SessionManagerImplTest : public ::testing::Test {
 public:
  SessionManagerImplTest()
      : device_policy_service_(new MockDevicePolicyService),
        state_key_generator_(&utils_, &metrics_),
        android_container_(kAndroidPid),
        system_clock_proxy_(new MockObjectProxy),
        impl_(base::MakeUnique<StubUpstartSignalEmitter>(
                  &upstart_signal_emitter_delegate_),
              &dbus_emitter_,
              base::Bind(&SessionManagerImplTest::FakeLockScreen,
                         base::Unretained(this)),
              base::Bind(&SessionManagerImplTest::FakeRestartDevice,
                         base::Unretained(this)),
              base::Bind(&SessionManagerImplTest::FakeStartArcInstance,
                         base::Unretained(this)),
              base::Bind(&SessionManagerImplTest::FakeStopArcInstance,
                         base::Unretained(this)),
              &key_gen_,
              &state_key_generator_,
              &manager_,
              &metrics_,
              &nss_,
              &utils_,
              &crossystem_,
              &vpd_process_,
              &owner_key_,
              &android_container_,
              &install_attributes_reader_,
              system_clock_proxy_.get()),
        fake_salt_("fake salt"),
        actual_locks_(0),
        expected_locks_(0),
        actual_restarts_(0),
        expected_restarts_(0) {}

  virtual ~SessionManagerImplTest() {}

  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    utils_.set_base_dir_for_testing(tmpdir_.path());
    SetSystemSalt(&fake_salt_);

#if USE_CHEETS
    android_data_dir_ =
        SessionManagerImpl::GetAndroidDataDirForUser(kSaneEmail);
    android_data_old_dir_ =
        SessionManagerImpl::GetAndroidDataOldDirForUser(kSaneEmail);
#endif  // USE_CHEETS

    // AtomicFileWrite calls in TEST_F assume that these directories exist.
    ASSERT_TRUE(utils_.CreateDir(
        base::FilePath(FILE_PATH_LITERAL("/run/session_manager"))));
    ASSERT_TRUE(utils_.CreateDir(
        base::FilePath(FILE_PATH_LITERAL("/mnt/stateful_partition"))));

    EXPECT_CALL(*(system_clock_proxy_.get()), WaitForServiceToBeAvailable(_))
        .WillRepeatedly(testing::SaveArg<0>(&available_callback_));
    EXPECT_CALL(*(system_clock_proxy_.get()),
                CallMethod(_, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillRepeatedly(testing::SaveArg<2>(&time_sync_callback_));

    auto factory = base::MakeUnique<MockUserPolicyServiceFactory>();
    EXPECT_CALL(*factory, Create(_))
        .WillRepeatedly(
            Invoke(this, &SessionManagerImplTest::CreateUserPolicyService));
    auto device_local_account_policy =
        base::MakeUnique<DeviceLocalAccountPolicyService>(tmpdir_.path(),
                                                          nullptr);
    impl_.SetPolicyServicesForTest(
        std::unique_ptr<DevicePolicyService>(device_policy_service_),
        std::move(factory),
        std::move(device_local_account_policy));

    SetDefaultMockBehavior();
    impl_.Initialize();
  }

  void TearDown() override {
    SetSystemSalt(NULL);
    EXPECT_EQ(actual_locks_, expected_locks_);
    EXPECT_EQ(actual_restarts_, expected_restarts_);
  }

 protected:
  void SetDeviceMode(const std::string& mode) {
    install_attributes_reader_.SetAttributes({{"enterprise.mode", mode}});
  }

  void ExpectStartSession(const string& account_id_string) {
    ExpectSessionBoilerplate(account_id_string, false, false);
  }

  void ExpectGuestSession() {
    ExpectSessionBoilerplate(kGuestUserName, true, false);
  }

  void ExpectStartOwnerSession(const string& account_id_string) {
    ExpectSessionBoilerplate(account_id_string, false, true);
  }

  void ExpectStartSessionUnowned(const string& account_id_string) {
    ExpectStartSessionUnownedBoilerplate(account_id_string,
                                         false,  // mitigating
                                         true);  // key_gen
  }

  void ExpectStartSessionOwningInProcess(const string& account_id_string) {
    ExpectStartSessionUnownedBoilerplate(account_id_string,
                                         false,   // mitigating
                                         false);  // key_gen
  }

  void ExpectStartSessionOwnerLost(const string& account_id_string) {
    ExpectStartSessionUnownedBoilerplate(account_id_string,
                                         true,    // mitigating
                                         false);  // key_gen
  }

  void ExpectStartSessionActiveDirectory(const string& account_id_string) {
    ExpectStartSessionUnownedBoilerplate(account_id_string,
                                         false,   // mitigating
                                         false);  // key_gen
  }
  void ExpectRemoveArcData(DataDirType data_dir_type,
                           OldDataDirType old_data_dir_type) {
#if USE_CHEETS
    if (data_dir_type == DataDirType::DATA_DIR_MISSING &&
        old_data_dir_type == OldDataDirType::OLD_DATA_DIR_EMPTY) {
      return;  // RemoveArcDataInternal does nothing in this case.
    }
    EXPECT_CALL(
        upstart_signal_emitter_delegate_,
        OnSignalEmitted(StrEq(SessionManagerImpl::kArcRemoveOldDataSignal),
                        ElementsAre(StartsWith("ANDROID_DATA_OLD_DIR="))))
        .Times(1);
#endif
  }

  void ExpectLockScreen() { expected_locks_ = 1; }

  void ExpectDeviceRestart() { expected_restarts_ = 1; }

  void ExpectStorePolicy(MockDevicePolicyService* service,
                         const std::vector<uint8_t>& policy_blob,
                         int flags,
                         SignatureCheck signature_check) {
    EXPECT_CALL(*service,
                Store(policy_blob, flags, signature_check, _))
        .WillOnce(Return(true));
  }

  void ExpectNoStorePolicy(MockDevicePolicyService* service) {
    EXPECT_CALL(*service, Store(_, _, _, _)).Times(0);
  }

  void ExpectAndRunStartSession(const string& email) {
    ExpectStartSession(email);
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.StartSession(&error, email, kNothing));
    EXPECT_FALSE(error.get());
    VerifyAndClearExpectations();
  }

  void ExpectAndRunGuestSession() {
    ExpectGuestSession();
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.StartSession(&error, kGuestUserName, kNothing));
    EXPECT_FALSE(error.get());
    VerifyAndClearExpectations();
  }

  PolicyService* CreateUserPolicyService(const string& username) {
    user_policy_services_[username] = new MockPolicyService();
    return user_policy_services_[username];
  }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(device_policy_service_);
    for (map<string, MockPolicyService*>::iterator it =
             user_policy_services_.begin();
         it != user_policy_services_.end(); ++it) {
      Mock::VerifyAndClearExpectations(it->second);
    }
    Mock::VerifyAndClearExpectations(&manager_);
    Mock::VerifyAndClearExpectations(&metrics_);
    Mock::VerifyAndClearExpectations(&nss_);
    Mock::VerifyAndClearExpectations(&utils_);
    Mock::VerifyAndClearExpectations(&upstart_signal_emitter_delegate_);

    // Reset the default mock behavior.
    SetDefaultMockBehavior();
  }

  void GotLastSyncInfo(bool network_synchronized) {
    ASSERT_TRUE(!available_callback_.is_null());
    available_callback_.Run(true);
    ASSERT_TRUE(!time_sync_callback_.is_null());
    std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendBool(network_synchronized);
    time_sync_callback_.Run(response.get());
  }

  // These are bare pointers, not unique_ptrs, because we need to give them
  // to a SessionManagerImpl instance, but also be able to set expectations
  // on them after we hand them off.
  MockDevicePolicyService* device_policy_service_;
  map<string, MockPolicyService*> user_policy_services_;

  MockDBusSignalEmitter dbus_emitter_;
  StubUpstartSignalEmitter::MockDelegate upstart_signal_emitter_delegate_;
  MockKeyGenerator key_gen_;
  MockServerBackedStateKeyGenerator state_key_generator_;
  MockProcessManagerService manager_;
  MockMetrics metrics_;
  MockNssUtil nss_;
  MockSystemUtils utils_;
  FakeCrossystem crossystem_;
  MockVpdProcess vpd_process_;
  MockPolicyKey owner_key_;
  FakeContainerManager android_container_;
  MockInstallAttributesReader install_attributes_reader_;
  scoped_refptr<MockObjectProxy> system_clock_proxy_;
  dbus::ObjectProxy::WaitForServiceToBeAvailableCallback available_callback_;
  dbus::ObjectProxy::ResponseCallback time_sync_callback_;

  // Used by fake closures to simulate doing whatever start/stop work needs
  // doing for ARC.
  bool arc_setup_completed_;

  SessionManagerImpl impl_;
  SessionManagerImpl::Error error_;
  base::ScopedTempDir tmpdir_;

#if USE_CHEETS
  base::FilePath android_data_dir_;
  base::FilePath android_data_old_dir_;
#endif  // USE_CHEETS

  static const pid_t kDummyPid;
  static const char kNothing[];
  static const char kSaneEmail[];
  static const char kContainerInstanceId[];
  static const int kAllKeyFlags;

 private:
  void SetDefaultMockBehavior() {
    // 10 GB Free Disk Space for ARC launch.
    EXPECT_CALL(utils_, AmountOfFreeDiskSpace(_))
        .WillRepeatedly(Return(10LL << 30));
    EXPECT_CALL(utils_, GetDevModeState())
        .WillRepeatedly(Return(DevModeState::DEV_MODE_OFF));
    EXPECT_CALL(utils_, GetVmState())
        .WillRepeatedly(Return(VmState::OUTSIDE_VM));
  }

  void ExpectSessionBoilerplate(const string& account_id_string,
                                bool guest,
                                bool for_owner) {
    EXPECT_CALL(manager_, SetBrowserSessionForUser(
                              StrEq(account_id_string),
                              StrEq(SanitizeUserName(account_id_string))))
        .Times(1);
    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(account_id_string), _, _, _))
        .WillOnce(DoAll(SetArgumentPointee<2>(for_owner), Return(true)));
    // Confirm that the key is present.
    EXPECT_CALL(*device_policy_service_, KeyMissing()).WillOnce(Return(false));

    EXPECT_CALL(metrics_, SendLoginUserType(false, guest, for_owner)).Times(1);
    EXPECT_CALL(
        upstart_signal_emitter_delegate_,
        OnSignalEmitted(StrEq(SessionManagerImpl::kStartUserSessionSignal),
                        ElementsAre(StartsWith("CHROMEOS_USER="))))
        .Times(1);
    EXPECT_CALL(
        dbus_emitter_,
        EmitSignalWithString(StrEq(login_manager::kSessionStateChangedSignal),
                             StrEq(SessionManagerImpl::kStarted)))
        .Times(1);
  }

  void ExpectStartSessionUnownedBoilerplate(const string& account_id_string,
                                            bool mitigating,
                                            bool key_gen) {
    CHECK(!(mitigating && key_gen));

    EXPECT_CALL(manager_, SetBrowserSessionForUser(
                              StrEq(account_id_string),
                              StrEq(SanitizeUserName(account_id_string))))
        .Times(1);

    // Expect initialization of the device policy service, return success.
    EXPECT_CALL(*device_policy_service_,
                CheckAndHandleOwnerLogin(StrEq(account_id_string), _, _, _))
        .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(true)));

    // Indicate that there is no owner key in order to trigger a new one to be
    // generated.
    EXPECT_CALL(*device_policy_service_, KeyMissing()).WillOnce(Return(true));
    EXPECT_CALL(*device_policy_service_, Mitigating())
        .WillRepeatedly(Return(mitigating));
    if (key_gen)
      EXPECT_CALL(key_gen_, Start(StrEq(account_id_string))).Times(1);
    else
      EXPECT_CALL(key_gen_, Start(_)).Times(0);

    EXPECT_CALL(metrics_, SendLoginUserType(false, false, false)).Times(1);
    EXPECT_CALL(
        upstart_signal_emitter_delegate_,
        OnSignalEmitted(StrEq(SessionManagerImpl::kStartUserSessionSignal),
                        ElementsAre(StartsWith("CHROMEOS_USER="))))
        .Times(1);
    EXPECT_CALL(
        dbus_emitter_,
        EmitSignalWithString(StrEq(login_manager::kSessionStateChangedSignal),
                             StrEq(SessionManagerImpl::kStarted)))
        .Times(1);
  }

  void FakeLockScreen() { actual_locks_++; }

  void FakeRestartDevice() { actual_restarts_++; }

  void FakeStartArcInstance() { arc_setup_completed_ = true; }

  void FakeStopArcInstance() { arc_setup_completed_ = false; }

  string fake_salt_;

  base::MessageLoop loop;

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
const int SessionManagerImplTest::kAllKeyFlags =
    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW |
    PolicyService::KEY_CLOBBER;

TEST_F(SessionManagerImplTest, EmitLoginPromptVisible) {
  const char event_name[] = "login-prompt-visible";
  EXPECT_CALL(metrics_, RecordStats(StrEq(event_name))).Times(1);
  EXPECT_CALL(dbus_emitter_,
              EmitSignal(StrEq(login_manager::kLoginPromptVisibleSignal)))
      .Times(1);
  impl_.EmitLoginPromptVisible();
}

TEST_F(SessionManagerImplTest, EnableChromeTesting) {
  std::vector<std::string> args = {"--repeat-arg", "--one-time-arg"};

  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("" /* ignored */, &temp_dir));

  const size_t random_suffix_len = strlen("XXXXXX");
  ASSERT_LT(random_suffix_len, temp_dir.value().size()) << temp_dir.value();

  // Check that RestartBrowserWithArgs() is called with a randomly chosen
  // --testing-channel path name.
  const string expected_testing_path_prefix =
      temp_dir.value().substr(0, temp_dir.value().size() - random_suffix_len);
  EXPECT_CALL(manager_,
              RestartBrowserWithArgs(
                  ElementsAre(args[0], args[1],
                              HasSubstr(expected_testing_path_prefix)),
                  true))
      .Times(1);

  {
    brillo::ErrorPtr error;
    std::string testing_path;
    ASSERT_TRUE(impl_.EnableChromeTesting(&error, false, args, &testing_path));
    EXPECT_FALSE(error.get());
    EXPECT_NE(std::string::npos,
              testing_path.find(expected_testing_path_prefix)) << testing_path;
  }

  // Calling again, without forcing relaunch, should not do anything.
  {
    brillo::ErrorPtr error;
    std::string testing_path;
    ASSERT_TRUE(impl_.EnableChromeTesting(&error, false, args, &testing_path));
    EXPECT_FALSE(error.get());
    EXPECT_NE(std::string::npos,
              testing_path.find(expected_testing_path_prefix)) << testing_path;
  }

  // Force relaunch.  Should go through the whole path again.
  args[0] = "--dummy";
  args[1] = "--repeat-arg";
  EXPECT_CALL(manager_,
              RestartBrowserWithArgs(
                  ElementsAre(args[0], args[1],
                              HasSubstr(expected_testing_path_prefix)),
                  true))
      .Times(1);

  {
    brillo::ErrorPtr error;
    std::string testing_path;
    ASSERT_TRUE(impl_.EnableChromeTesting(&error, true, args, &testing_path));
    EXPECT_FALSE(error.get());
    EXPECT_NE(std::string::npos,
              testing_path.find(expected_testing_path_prefix)) << testing_path;
  }
}

TEST_F(SessionManagerImplTest, StartSession) {
  ExpectStartSession(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
}

TEST_F(SessionManagerImplTest, StartSession_New) {
  ExpectStartSessionUnowned(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
}

TEST_F(SessionManagerImplTest, StartSession_InvalidUser) {
  constexpr char kBadEmail[] = "user";
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.StartSession(&error, kBadEmail, kNothing));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kInvalidAccount, error->GetCode());
}

TEST_F(SessionManagerImplTest, StartSession_Twice) {
  ExpectStartSession(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());

  EXPECT_FALSE(impl_.StartSession(&error, kSaneEmail, kNothing));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kSessionExists, error->GetCode());
}

TEST_F(SessionManagerImplTest, StartSession_TwoUsers) {
  ExpectStartSession(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());
  VerifyAndClearExpectations();

  constexpr char kEmail2[] = "user2@somewhere";
  ExpectStartSession(kEmail2);
  EXPECT_TRUE(impl_.StartSession(&error, kEmail2, kNothing));
  EXPECT_FALSE(error.get());
}

TEST_F(SessionManagerImplTest, StartSession_OwnerAndOther) {
  ExpectStartSessionUnowned(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());
  VerifyAndClearExpectations();

  constexpr char kEmail2[] = "user2@somewhere";
  ExpectStartSession(kEmail2);
  EXPECT_TRUE(impl_.StartSession(&error, kEmail2, kNothing));
  EXPECT_FALSE(error.get());
}

TEST_F(SessionManagerImplTest, StartSession_OwnerRace) {
  ExpectStartSessionUnowned(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());
  VerifyAndClearExpectations();

  constexpr char kEmail2[] = "user2@somewhere";
  ExpectStartSessionOwningInProcess(kEmail2);
  EXPECT_TRUE(impl_.StartSession(&error, kEmail2, kNothing));
  EXPECT_FALSE(error.get());
}

TEST_F(SessionManagerImplTest, StartSession_BadNssDB) {
  nss_.MakeBadDB();
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.StartSession(&error, kSaneEmail, kNothing));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kNoUserNssDb, error->GetCode());
}

TEST_F(SessionManagerImplTest, StartSession_DevicePolicyFailure) {
  // Upon the owner login check, return an error.

  EXPECT_CALL(*device_policy_service_,
              CheckAndHandleOwnerLogin(StrEq(kSaneEmail), _, _, _))
      .WillOnce(WithArg<3>(Invoke([](brillo::ErrorPtr* error) {
              *error = CreateError(dbus_error::kPubkeySetIllegal, "test");
              return false;
            })));

  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.StartSession(&error, kSaneEmail, kNothing));
  ASSERT_TRUE(error.get());
}

TEST_F(SessionManagerImplTest, StartSession_Owner) {
  ExpectStartOwnerSession(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());
}

TEST_F(SessionManagerImplTest, StartSession_KeyMitigation) {
  ExpectStartSessionOwnerLost(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());
}

// Ensure that starting Active Directory session does not create owner key.
TEST_F(SessionManagerImplTest, StartSession_ActiveDirectorManaged) {
  SetDeviceMode("enterprise_ad");
  ExpectStartSessionActiveDirectory(kSaneEmail);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());
}

TEST_F(SessionManagerImplTest, StopSession) {
  EXPECT_CALL(manager_, ScheduleShutdown()).Times(1);
  impl_.StopSession("");
}

TEST_F(SessionManagerImplTest, StorePolicy_NoSession) {
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  ExpectStorePolicy(device_policy_service_, policy_blob, kAllKeyFlags,
                    SignatureCheck::kEnabled);
  impl_.StorePolicy(policy_blob, SignatureCheck::kEnabled,
                    MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, StorePolicy_SessionStarted) {
  ExpectAndRunStartSession(kSaneEmail);
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  ExpectStorePolicy(device_policy_service_, policy_blob,
                    PolicyService::KEY_ROTATE, SignatureCheck::kEnabled);
  impl_.StorePolicy(policy_blob, SignatureCheck::kEnabled,
                    MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, StorePolicy_NoSignatureConsumer) {
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  ExpectNoStorePolicy(device_policy_service_);
  impl_.StorePolicy(policy_blob, SignatureCheck::kDisabled,
                    MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, StorePolicy_NoSignatureEnterprise) {
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  SetDeviceMode("enterprise");
  ExpectNoStorePolicy(device_policy_service_);
  impl_.StorePolicy(policy_blob, SignatureCheck::kDisabled,
                    MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, StorePolicy_NoSignatureEnterpriseAD) {
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  SetDeviceMode("enterprise_ad");
  ExpectStorePolicy(device_policy_service_, policy_blob, kAllKeyFlags,
                    SignatureCheck::kDisabled);
  impl_.StorePolicy(policy_blob, SignatureCheck::kDisabled,
                    MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, RetrievePolicy) {
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  EXPECT_CALL(*device_policy_service_, Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_blob), Return(true)));
  std::vector<uint8_t> out_blob;
  impl_.RetrievePolicy(&out_blob, NULL);

  EXPECT_EQ(policy_blob, out_blob);
}

namespace {

void CaptureError(brillo::ErrorPtr* storage, brillo::ErrorPtr error) {
  DCHECK(storage);
  *storage = std::move(error);
}

void HandleStateKeys(const std::vector<std::vector<uint8_t>>& state_keys) {}
}  // namespace

TEST_F(SessionManagerImplTest, RequestStateKeys_TimeSync) {
  EXPECT_CALL(state_key_generator_, RequestStateKeys(_));
  impl_.RequestServerBackedStateKeys(base::Bind(&HandleStateKeys));
  GotLastSyncInfo(true);
}

TEST_F(SessionManagerImplTest, RequestStateKeys_NoTimeSync) {
  EXPECT_CALL(state_key_generator_, RequestStateKeys(_)).Times(0);
  impl_.RequestServerBackedStateKeys(base::Bind(&HandleStateKeys));
}

TEST_F(SessionManagerImplTest, RequestStateKeys_TimeSyncDoneBefore) {
  GotLastSyncInfo(true);
  EXPECT_CALL(state_key_generator_, RequestStateKeys(_));
  impl_.RequestServerBackedStateKeys(base::Bind(&HandleStateKeys));
}

TEST_F(SessionManagerImplTest, RequestStateKeys_FailedTimeSync) {
  EXPECT_CALL(state_key_generator_, RequestStateKeys(_)).Times(0);
  GotLastSyncInfo(false);
  impl_.RequestServerBackedStateKeys(base::Bind(&HandleStateKeys));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SessionManagerImplTest, RequestStateKeys_TimeSyncAfterFail) {
  GotLastSyncInfo(false);
  impl_.RequestServerBackedStateKeys(base::Bind(&HandleStateKeys));
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(state_key_generator_, RequestStateKeys(_));
  GotLastSyncInfo(true);
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_NoSession) {
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  brillo::ErrorPtr error;
  impl_.StorePolicyForUser(kSaneEmail, policy_blob, SignatureCheck::kEnabled,
                           base::Bind(&CaptureError, &error));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kSessionDoesNotExist, error->GetCode());
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_SessionStarted) {
  ExpectAndRunStartSession(kSaneEmail);
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  EXPECT_CALL(*user_policy_services_[kSaneEmail],
              Store(policy_blob,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW,
                    SignatureCheck::kEnabled, _))
      .WillOnce(Return(true));
  impl_.StorePolicyForUser(kSaneEmail, policy_blob, SignatureCheck::kEnabled,
                           MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_SecondSession) {
  ExpectAndRunStartSession(kSaneEmail);
  ASSERT_TRUE(user_policy_services_[kSaneEmail]);

  // Store policy for the signed-in user.
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  EXPECT_CALL(*user_policy_services_[kSaneEmail],
              Store(policy_blob,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW,
                    SignatureCheck::kEnabled, _))
      .WillOnce(Return(true));
  impl_.StorePolicyForUser(kSaneEmail, policy_blob,
                           SignatureCheck::kEnabled,
                           MockPolicyService::CreateDoNothing());
  Mock::VerifyAndClearExpectations(user_policy_services_[kSaneEmail]);

  // Storing policy for another username fails before their session starts.
  constexpr char kEmail2[] = "user2@somewhere.com";
  brillo::ErrorPtr error;
  impl_.StorePolicyForUser(kEmail2, policy_blob, SignatureCheck::kEnabled,
                           base::Bind(&CaptureError, &error));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kSessionDoesNotExist, error->GetCode());

  // Now start another session for the 2nd user.
  ExpectAndRunStartSession(kEmail2);
  ASSERT_TRUE(user_policy_services_[kEmail2]);

  // Storing policy for that user now succeeds.
  EXPECT_CALL(*user_policy_services_[kEmail2],
              Store(policy_blob,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW,
                    SignatureCheck::kEnabled, _))
      .WillOnce(Return(true));
  impl_.StorePolicyForUser(kEmail2, policy_blob, SignatureCheck::kEnabled,
                           MockPolicyService::CreateDoNothing());
  Mock::VerifyAndClearExpectations(user_policy_services_[kEmail2]);
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_NoSignatureConsumer) {
  ExpectAndRunStartSession(kSaneEmail);
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  EXPECT_CALL(*user_policy_services_[kSaneEmail], Store(_, _, _, _))
      .Times(0);
  impl_.StorePolicyForUser(kSaneEmail, policy_blob, SignatureCheck::kDisabled,
                           MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_NoSignatureEnterprise) {
  ExpectAndRunStartSession(kSaneEmail);
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  SetDeviceMode("enterprise");
  EXPECT_CALL(*user_policy_services_[kSaneEmail], Store(_, _, _, _))
      .Times(0);
  impl_.StorePolicyForUser(kSaneEmail, policy_blob, SignatureCheck::kDisabled,
                           MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, StoreUserPolicy_NoSignatureEnterpriseAD) {
  ExpectAndRunStartSession(kSaneEmail);
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  SetDeviceMode("enterprise_ad");
  EXPECT_CALL(*user_policy_services_[kSaneEmail],
              Store(policy_blob,
                    PolicyService::KEY_ROTATE | PolicyService::KEY_INSTALL_NEW,
                    SignatureCheck::kDisabled, _))
      .WillOnce(Return(true));
  impl_.StorePolicyForUser(kSaneEmail, policy_blob, SignatureCheck::kDisabled,
                           MockPolicyService::CreateDoNothing());
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicy_NoSession) {
  std::vector<uint8_t> out_blob;
  impl_.RetrievePolicyForUser(kSaneEmail, &out_blob, &error_);
  EXPECT_EQ(out_blob.size(), 0);
  EXPECT_EQ(dbus_error::kSessionDoesNotExist, error_.name());
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicy_SessionStarted) {
  ExpectAndRunStartSession(kSaneEmail);
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  EXPECT_CALL(*user_policy_services_[kSaneEmail], Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_blob), Return(true)));
  std::vector<uint8_t> out_blob;
  impl_.RetrievePolicyForUser(kSaneEmail, &out_blob, &error_);
  EXPECT_EQ(policy_blob, out_blob);
}

TEST_F(SessionManagerImplTest, RetrieveUserPolicy_SecondSession) {
  ExpectAndRunStartSession(kSaneEmail);
  ASSERT_TRUE(user_policy_services_[kSaneEmail]);

  // Retrieve policy for the signed-in user.
  const std::vector<uint8_t> policy_blob = StringToBlob("fake policy");
  EXPECT_CALL(*user_policy_services_[kSaneEmail], Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_blob), Return(true)));
  {
    std::vector<uint8_t> out_blob;
    impl_.RetrievePolicyForUser(kSaneEmail, &out_blob, NULL);
    Mock::VerifyAndClearExpectations(user_policy_services_[kSaneEmail]);
    EXPECT_EQ(policy_blob, out_blob);
  }

  // Retrieving policy for another username fails before their session starts.
  constexpr char kEmail2[] = "user2@somewhere.com";
  {
    std::vector<uint8_t> out_blob;
    impl_.RetrievePolicyForUser(kEmail2, &out_blob, &error_);
    EXPECT_EQ(error_.name(), dbus_error::kSessionDoesNotExist);
  }

  // Now start another session for the 2nd user.
  ExpectAndRunStartSession(kEmail2);
  ASSERT_TRUE(user_policy_services_[kEmail2]);

  // Retrieving policy for that user now succeeds.
  EXPECT_CALL(*user_policy_services_[kEmail2], Retrieve(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(policy_blob), Return(true)));
  {
    std::vector<uint8_t> out_blob;
    impl_.RetrievePolicyForUser(kEmail2, &out_blob, NULL);
    Mock::VerifyAndClearExpectations(user_policy_services_[kEmail2]);
    EXPECT_EQ(policy_blob, out_blob);
  }
}

TEST_F(SessionManagerImplTest, RetrieveActiveSessions) {
  ExpectStartSession(kSaneEmail);
  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
    EXPECT_FALSE(error.get());
  }
  {
    std::map<std::string, std::string> active_users =
        impl_.RetrieveActiveSessions();
    EXPECT_EQ(active_users.size(), 1);
    EXPECT_EQ(active_users[kSaneEmail], SanitizeUserName(kSaneEmail));
  }
  VerifyAndClearExpectations();

  constexpr char kEmail2[] = "user2@somewhere";
  ExpectStartSession(kEmail2);
  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.StartSession(&error, kEmail2, kNothing));
    EXPECT_FALSE(error.get());
  }
  {
    std::map<std::string, std::string> active_users =
        impl_.RetrieveActiveSessions();
    EXPECT_EQ(active_users.size(), 2);
    EXPECT_EQ(active_users[kSaneEmail], SanitizeUserName(kSaneEmail));
    EXPECT_EQ(active_users[kEmail2], SanitizeUserName(kEmail2));
  }
}

TEST_F(SessionManagerImplTest, RestartJobBadSocket) {
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.RestartJob(&error, dbus::FileDescriptor(), {}));
  ASSERT_TRUE(error.get());
  EXPECT_EQ("GetPeerCredsFailed", error->GetCode());
}

TEST_F(SessionManagerImplTest, RestartJobBadPid) {
  int sockets[2] = {-1, -1};
  ASSERT_GE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  base::ScopedFD fd0_closer(sockets[0]);
  dbus::FileDescriptor fd1;
  fd1.PutValue(sockets[1]);
  fd1.CheckValidity();

  EXPECT_CALL(manager_, IsBrowser(getpid())).WillRepeatedly(Return(false));
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.RestartJob(&error, fd1, {}));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kUnknownPid, error->GetCode());
}

TEST_F(SessionManagerImplTest, RestartJobSuccess) {
  int sockets[2] = {-1, -1};
  ASSERT_GE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  base::ScopedFD fd0_closer(sockets[0]);
  dbus::FileDescriptor fd1;
  fd1.PutValue(sockets[1]);
  fd1.CheckValidity();

  const std::vector<std::string> argv = {
      "program",
      "--switch1",
      "--switch2=switch2_value",
      "--switch3=escaped_\"_quote",
      "--switch4=white space",
      "arg1",
      "arg 2",
  };

  EXPECT_CALL(manager_, IsBrowser(getpid())).WillRepeatedly(Return(true));
  EXPECT_CALL(manager_, RestartBrowserWithArgs(ElementsAreArray(argv), false))
      .Times(1);
  ExpectGuestSession();

  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RestartJob(&error, fd1, argv));
  EXPECT_FALSE(error.get());
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
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.LockScreen(&error));
  EXPECT_FALSE(error.get());
  EXPECT_TRUE(impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockScreen_DuringSupervisedUserCreation) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectLockScreen();
  EXPECT_CALL(dbus_emitter_, EmitSignal(_)).Times(AnyNumber());

  impl_.HandleSupervisedUserCreationStarting();
  EXPECT_TRUE(impl_.ShouldEndSession());
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.LockScreen(&error));
  EXPECT_FALSE(error.get());
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
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.LockScreen(&error));
  EXPECT_FALSE(error.get());
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
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.LockScreen(&error));
  EXPECT_FALSE(error.get());
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockScreen_NoSession) {
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.LockScreen(&error));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kSessionDoesNotExist, error->GetCode());
}

TEST_F(SessionManagerImplTest, LockScreen_Guest) {
  ExpectAndRunGuestSession();
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.LockScreen(&error));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kSessionExists, error->GetCode());
}

TEST_F(SessionManagerImplTest, LockScreen_UserAndGuest) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectAndRunGuestSession();
  ExpectLockScreen();
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.LockScreen(&error));
  ASSERT_FALSE(error.get());
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, LockUnlockScreen) {
  ExpectAndRunStartSession(kSaneEmail);
  ExpectLockScreen();
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.LockScreen(&error));
  EXPECT_FALSE(error.get());
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());

  EXPECT_CALL(dbus_emitter_,
              EmitSignal(StrEq(login_manager::kScreenIsLockedSignal)))
      .Times(1);
  impl_.HandleLockScreenShown();
  EXPECT_EQ(TRUE, impl_.ShouldEndSession());

  EXPECT_CALL(dbus_emitter_,
              EmitSignal(StrEq(login_manager::kScreenIsUnlockedSignal)))
      .Times(1);
  impl_.HandleLockScreenDismissed();
  EXPECT_EQ(FALSE, impl_.ShouldEndSession());
}

TEST_F(SessionManagerImplTest, StartDeviceWipe) {
  // Just make sure the device is being restart as sanity check of
  // InitiateDeviceWipe() invocation.
  ExpectDeviceRestart();

  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.StartDeviceWipe(&error));
  EXPECT_FALSE(error.get());
}

TEST_F(SessionManagerImplTest, StartDeviceWipe_AlreadyLoggedIn) {
  base::FilePath logged_in_path(SessionManagerImpl::kLoggedInFlag);
  ASSERT_FALSE(utils_.Exists(logged_in_path));
  ASSERT_TRUE(utils_.AtomicFileWrite(logged_in_path, "1"));
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.StartDeviceWipe(&error));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kSessionExists, error->GetCode());
}

TEST_F(SessionManagerImplTest, InitiateDeviceWipe_TooLongReason) {
  ASSERT_TRUE(utils_.RemoveFile(
      base::FilePath(SessionManagerImpl::kLoggedInFlag)));
  ExpectDeviceRestart();
  impl_.InitiateDeviceWipe(
      "overly long test message with\nspecial/chars$\t\xa4\xd6 1234567890");
  std::string contents;
  base::FilePath reset_path = utils_.PutInsideBaseDirForTesting(
      base::FilePath(SessionManagerImpl::kResetFile));
  ASSERT_TRUE(base::ReadFileToString(reset_path, &contents));
  ASSERT_EQ(
      "fast safe keepimg reason="
      "overly_long_test_message_with_special_chars_____12",
      contents);
}

TEST_F(SessionManagerImplTest, ImportValidateAndStoreGeneratedKey) {
  base::FilePath key_file_path;
  string key("key_contents");
  ASSERT_TRUE(base::CreateTemporaryFileInDir(tmpdir_.path(), &key_file_path));
  ASSERT_EQ(base::WriteFile(key_file_path, key.c_str(), key.size()),
            key.size());

  // Start a session, to set up NSSDB for the user.
  ExpectStartOwnerSession(kSaneEmail);
  brillo::ErrorPtr error;
  ASSERT_TRUE(impl_.StartSession(&error, kSaneEmail, kNothing));
  EXPECT_FALSE(error.get());

  EXPECT_CALL(*device_policy_service_,
              ValidateAndStoreOwnerKey(
                  StrEq(kSaneEmail), StringToBlob(key), nss_.GetSlot()))
      .WillOnce(Return(true));

  impl_.OnKeyGenerated(kSaneEmail, key_file_path);
  EXPECT_FALSE(base::PathExists(key_file_path));
}

TEST_F(SessionManagerImplTest, ContainerStart) {
  const std::string kContainerName = "testc";

  ExpectAndRunStartSession(kSaneEmail);

  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.StartContainer(&error, kContainerName));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kContainerStartupFail, error->GetCode());
  EXPECT_FALSE(android_container_.running());
}

TEST_F(SessionManagerImplTest, ArcInstanceStartForLoginScreen) {
#if USE_CHEETS
  {
    int64_t start_time = 0;
    brillo::ErrorPtr error;
    EXPECT_FALSE(impl_.GetArcStartTimeTicks(&error, &start_time));
    ASSERT_TRUE(error.get());
    EXPECT_EQ(dbus_error::kNotStarted, error->GetCode());
  }

  EXPECT_CALL(
      upstart_signal_emitter_delegate_,
      OnSignalEmitted(StrEq(SessionManagerImpl::kArcStartForLoginScreenSignal),
                      ElementsAre("CHROMEOS_DEV_MODE=0",
                                  "CHROMEOS_INSIDE_VM=0")))
      .Times(1);

  std::string container_instance_id;
  impl_.StartArcInstanceForLoginScreen(&container_instance_id, &error_);
  EXPECT_FALSE(container_instance_id.empty());
  EXPECT_TRUE(android_container_.running());
  EXPECT_TRUE(arc_setup_completed_);

  // StartArcInstanceForLoginScreen() does not update start time.
  {
    brillo::ErrorPtr error;
    int64_t start_time = 0;
    EXPECT_FALSE(impl_.GetArcStartTimeTicks(&error, &start_time));
    ASSERT_TRUE(error.get());
    EXPECT_EQ(dbus_error::kNotStarted, error->GetCode());
  }

  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcStopSignal),
                              ElementsAre()))
      .Times(1);
  // StartArcInstanceForLoginScreen does not emit kArcNetworkStartSignal. Its
  // OnStop closure does emit kArcNetworkStopSignal but Upstart will ignore it.
  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcNetworkStopSignal),
                              ElementsAre()))
      .Times(1);
  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.StopArcInstance(&error));
    EXPECT_FALSE(error.get());
  }
  EXPECT_FALSE(android_container_.running());
  EXPECT_FALSE(arc_setup_completed_);
#else
  std::string container_instance_id;
  impl_.StartArcInstanceForLoginScreen(&container_instance_id, &error_);
  EXPECT_TRUE(container_instance_id.empty());
  EXPECT_EQ(dbus_error::kNotAvailable, error_.name());
  {
    brillo::ErrorPtr error;
    int64_t start_time = 0;
    EXPECT_FALSE(impl_.GetArcStartTimeTicks(&error, &start_time));
    ASSERT_TRUE(error.get());
    EXPECT_EQ(dbus_error::kNotAvailable, error->GetCode());
  }
#endif
}

TEST_F(SessionManagerImplTest, ArcInstanceStart) {
  ExpectAndRunStartSession(kSaneEmail);
#if USE_CHEETS
  {
    brillo::ErrorPtr error;
    int64_t start_time = 0;
    EXPECT_FALSE(impl_.GetArcStartTimeTicks(&error, &start_time));
    ASSERT_TRUE(error.get());
    EXPECT_EQ(dbus_error::kNotStarted, error->GetCode());
  }

  EXPECT_CALL(
      upstart_signal_emitter_delegate_,
      OnSignalEmitted(StrEq(SessionManagerImpl::kArcStartSignal),
                      ElementsAre(StartsWith("ANDROID_DATA_DIR="),
                                  StartsWith("ANDROID_DATA_OLD_DIR="),
                                  std::string("CHROMEOS_USER=") + kSaneEmail,
                                  "CHROMEOS_DEV_MODE=0", "CHROMEOS_INSIDE_VM=0",
                                  "DISABLE_BOOT_COMPLETED_BROADCAST=0",
                                  "ENABLE_VENDOR_PRIVILEGED=1")))
      .Times(1);
  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcStopSignal),
                              ElementsAre()))
      .Times(1);
  EXPECT_CALL(
      upstart_signal_emitter_delegate_,
      OnSignalEmitted(StrEq(SessionManagerImpl::kArcNetworkStartSignal),
                      ElementsAre(std::string("CONTAINER_NAME=") +
                                      SessionManagerImpl::kArcContainerName,
                                  std::string("CONTAINER_PATH="),
                                  std::string("CONTAINER_PID=") +
                                      std::to_string(kAndroidPid))))
      .Times(1);
  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcNetworkStopSignal),
                              ElementsAre()))
      .Times(1);
  std::string container_instance_id;
  impl_.StartArcInstance(
      kSaneEmail, false, true, &container_instance_id, &error_);
  EXPECT_FALSE(container_instance_id.empty());
  EXPECT_TRUE(android_container_.running());
  EXPECT_TRUE(arc_setup_completed_);
  {
    brillo::ErrorPtr error;
    int64_t start_time = 0;
    EXPECT_TRUE(impl_.GetArcStartTimeTicks(&error, &start_time));
    EXPECT_NE(0, start_time);
    ASSERT_FALSE(error.get());
  }
  EXPECT_CALL(
      dbus_emitter_,
      EmitSignalWithBoolAndString(StrEq(login_manager::kArcInstanceStopped),
                                  true,
                                  StrEq(container_instance_id)))
      .Times(1);

  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.StopArcInstance(&error));
    EXPECT_FALSE(error.get());
  }
  EXPECT_FALSE(android_container_.running());
  EXPECT_FALSE(arc_setup_completed_);
#else
  std::string container_instance_id;
  impl_.StartArcInstance(
      kSaneEmail, false, false, &container_instance_id, &error_);
  EXPECT_TRUE(container_instance_id.empty());
  EXPECT_EQ(dbus_error::kNotAvailable, error_.name());
  {
    brillo::ErrorPtr error;
    int64_t start_time = 0;
    EXPECT_FALSE(impl_.GetArcStartTimeTicks(&error, &start_time));
    ASSERT_TRUE(error.get());
    EXPECT_EQ(dbus_error::kNotAvailable, error->GetCode());
  }
#endif
}

#if USE_CHEETS
TEST_F(SessionManagerImplTest, ArcInstanceStart_NoSession) {
  std::string container_instance_id;
  impl_.StartArcInstance(
      kSaneEmail, false, false, &container_instance_id, &error_);
  EXPECT_TRUE(container_instance_id.empty());
  EXPECT_EQ(dbus_error::kSessionDoesNotExist, error_.name());
}

TEST_F(SessionManagerImplTest, ArcInstanceStart_LowDisk) {
  ExpectAndRunStartSession(kSaneEmail);

  // No free disk space.
  EXPECT_CALL(utils_, AmountOfFreeDiskSpace(_)).WillRepeatedly(Return(0));
  std::string container_instance_id;
  impl_.StartArcInstance(
      kSaneEmail, false, false, &container_instance_id, &error_);
  EXPECT_TRUE(container_instance_id.empty());
  EXPECT_EQ(dbus_error::kLowFreeDisk, error_.name());
}

TEST_F(SessionManagerImplTest, ArcInstanceCrash) {
  ExpectAndRunStartSession(kSaneEmail);

  EXPECT_CALL(
      upstart_signal_emitter_delegate_,
      OnSignalEmitted(StrEq(SessionManagerImpl::kArcStartSignal),
                      ElementsAre(StartsWith("ANDROID_DATA_DIR="),
                                  StartsWith("ANDROID_DATA_OLD_DIR="),
                                  std::string("CHROMEOS_USER=") + kSaneEmail,
                                  "CHROMEOS_DEV_MODE=1", "CHROMEOS_INSIDE_VM=0",
                                  "DISABLE_BOOT_COMPLETED_BROADCAST=0",
                                  "ENABLE_VENDOR_PRIVILEGED=0")))
      .Times(1);
  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcStopSignal),
                              ElementsAre()))
      .Times(1);
  EXPECT_CALL(
      upstart_signal_emitter_delegate_,
      OnSignalEmitted(StrEq(SessionManagerImpl::kArcNetworkStartSignal),
                      ElementsAre(std::string("CONTAINER_NAME=") +
                                      SessionManagerImpl::kArcContainerName,
                                  std::string("CONTAINER_PATH="),
                                  std::string("CONTAINER_PID=") +
                                      std::to_string(kAndroidPid))))
      .Times(1);
  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcNetworkStopSignal),
                              ElementsAre()))
      .Times(1);
  // Overrides dev mode state.
  EXPECT_CALL(utils_, GetDevModeState())
      .WillOnce(Return(DevModeState::DEV_MODE_ON))
      .RetiresOnSaturation();
  std::string container_instance_id;
  impl_.StartArcInstance(
      kSaneEmail, false, false, &container_instance_id, &error_);
  EXPECT_FALSE(container_instance_id.empty());
  EXPECT_TRUE(android_container_.running());
  EXPECT_TRUE(arc_setup_completed_);
  EXPECT_CALL(
      dbus_emitter_,
      EmitSignalWithBoolAndString(StrEq(login_manager::kArcInstanceStopped),
                                  false,
                                  StrEq(container_instance_id)))
      .Times(1);

  android_container_.SimulateCrash();
  EXPECT_FALSE(android_container_.running());
  EXPECT_FALSE(arc_setup_completed_);
  // This should now fail since the container was cleaned up already.
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.StopArcInstance(&error));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kContainerShutdownFail, error->GetCode());
}

TEST_F(SessionManagerImplTest, ArcRemoveData) {
  // Test that RemoveArcData() removes |android_data_dir_| and reports success
  // even if the directory is not empty.
  ASSERT_TRUE(utils_.CreateDir(android_data_dir_));
  ASSERT_TRUE(utils_.AtomicFileWrite(android_data_dir_.Append("foo"), "test"));
  ASSERT_FALSE(utils_.Exists(android_data_old_dir_));
  ExpectRemoveArcData(DataDirType::DATA_DIR_AVAILABLE,
                      OldDataDirType::OLD_DATA_DIR_EMPTY);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
  EXPECT_FALSE(error.get());
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest, ArcRemoveData_NoSourceDirectory) {
  // Test that RemoveArcData() reports success when the directory does not
  // exist.
  ASSERT_FALSE(utils_.Exists(android_data_dir_));
  ASSERT_FALSE(utils_.Exists(android_data_old_dir_));
  ExpectRemoveArcData(DataDirType::DATA_DIR_MISSING,
                      OldDataDirType::OLD_DATA_DIR_EMPTY);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
  EXPECT_FALSE(error.get());
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest, ArcRemoveData_OldDirectoryExists) {
  // Test that RemoveArcData() can remove |android_data_dir_| and
  // reports success even if the "old" directory already exists.
  ASSERT_TRUE(utils_.CreateDir(android_data_dir_));
  ASSERT_TRUE(utils_.AtomicFileWrite(android_data_dir_.Append("foo"), "test"));
  ASSERT_TRUE(utils_.CreateDir(android_data_old_dir_));
  ExpectRemoveArcData(DataDirType::DATA_DIR_AVAILABLE,
                      OldDataDirType::OLD_DATA_DIR_EMPTY);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
  EXPECT_FALSE(error.get());
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest, ArcRemoveData_NonEmptyOldDirectoryExists) {
  // Test that RemoveArcData() can remove |android_data_dir_| and
  // reports success even if the "old" directory already exists and is not
  // empty.
  ASSERT_TRUE(utils_.CreateDir(android_data_dir_));
  ASSERT_TRUE(utils_.AtomicFileWrite(android_data_dir_.Append("foo"), "test"));
  ASSERT_TRUE(utils_.CreateDir(android_data_old_dir_));
  ASSERT_TRUE(
      utils_.AtomicFileWrite(android_data_old_dir_.Append("bar"), "test2"));
  ExpectRemoveArcData(DataDirType::DATA_DIR_AVAILABLE,
                      OldDataDirType::OLD_DATA_DIR_NOT_EMPTY);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
  EXPECT_FALSE(error.get());
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest,
       ArcRemoveData_NoSourceDirectoryButOldDirectoryExists) {
  // Test that RemoveArcData() removes the "old" directory and reports success
  // even when |android_data_dir_| does not exist at all.
  ASSERT_FALSE(utils_.Exists(android_data_dir_));
  ASSERT_TRUE(utils_.CreateDir(android_data_old_dir_));
  ExpectRemoveArcData(DataDirType::DATA_DIR_MISSING,
                      OldDataDirType::OLD_DATA_DIR_EMPTY);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
  EXPECT_FALSE(error.get());
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest,
       ArcRemoveData_NoSourceDirectoryButNonEmptyOldDirectoryExists) {
  // Test that RemoveArcData() removes the "old" directory and returns
  // true even when |android_data_dir_| does not exist at all.
  ASSERT_FALSE(utils_.Exists(android_data_dir_));
  ASSERT_TRUE(utils_.CreateDir(android_data_old_dir_));
  ASSERT_TRUE(
      utils_.AtomicFileWrite(android_data_old_dir_.Append("foo"), "test"));
  ExpectRemoveArcData(DataDirType::DATA_DIR_MISSING,
                      OldDataDirType::OLD_DATA_DIR_NOT_EMPTY);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
  EXPECT_FALSE(error.get());
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest, ArcRemoveData_OldFileExists) {
  // Test that RemoveArcData() can remove |android_data_dir_| and
  // returns true even if the "old" path exists as a file. This should never
  // happen, but RemoveArcData() can handle the case.
  ASSERT_TRUE(utils_.CreateDir(android_data_dir_));
  ASSERT_TRUE(utils_.AtomicFileWrite(android_data_dir_.Append("foo"), "test"));
  ASSERT_TRUE(utils_.AtomicFileWrite(android_data_old_dir_, "test2"));
  ExpectRemoveArcData(DataDirType::DATA_DIR_AVAILABLE,
                      OldDataDirType::OLD_DATA_FILE_EXISTS);
  brillo::ErrorPtr error;
  EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
  EXPECT_FALSE(error.get());
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest, ArcRemoveData_ArcRunning) {
  // Test that RemoveArcData does nothing when ARC is running.
  ExpectAndRunStartSession(kSaneEmail);
  ASSERT_TRUE(utils_.CreateDir(android_data_dir_));
  ASSERT_TRUE(utils_.AtomicFileWrite(android_data_dir_.Append("foo"), "test"));
  ASSERT_FALSE(utils_.Exists(android_data_old_dir_));
  std::string container_instance_id;
  impl_.StartArcInstance(
      kSaneEmail, false, false, &container_instance_id, &error_);
  EXPECT_FALSE(container_instance_id.empty());
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.RemoveArcData(&error, kSaneEmail));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kArcInstanceRunning, error->GetCode());
  EXPECT_TRUE(utils_.Exists(android_data_dir_));
}

TEST_F(SessionManagerImplTest, ArcRemoveData_ArcStopped) {
  ExpectAndRunStartSession(kSaneEmail);
  ASSERT_TRUE(utils_.CreateDir(android_data_dir_));
  ASSERT_TRUE(utils_.AtomicFileWrite(android_data_dir_.Append("foo"), "test"));
  ASSERT_TRUE(utils_.CreateDir(android_data_old_dir_));
  ASSERT_TRUE(
      utils_.AtomicFileWrite(android_data_old_dir_.Append("bar"), "test2"));
  std::string container_instance_id;
  impl_.StartArcInstance(
      kSaneEmail, false, false, &container_instance_id, &error_);
  EXPECT_FALSE(container_instance_id.empty());
  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.StopArcInstance(&error));
    EXPECT_FALSE(error.get());
  }
  ExpectRemoveArcData(DataDirType::DATA_DIR_AVAILABLE,
                      OldDataDirType::OLD_DATA_DIR_NOT_EMPTY);
  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.RemoveArcData(&error, kSaneEmail));
    EXPECT_FALSE(error.get());
  }
  EXPECT_FALSE(utils_.Exists(android_data_dir_));
}
#else
// When USE_CHEETS is not defined, ArcRemoveData should immediately return
// dbus_error::kNotAvailable.
TEST_F(SessionManagerImplTest, ArcRemoveData) {
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.RemoveArcData(&error, kSaneEmail));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kNotAvailable, error->GetCode());
}
#endif

TEST_F(SessionManagerImplTest, SetArcCpuRestrictionFails) {
#if USE_CHEETS
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.SetArcCpuRestriction(
      &error, static_cast<uint32_t>(NUM_CONTAINER_CPU_RESTRICTION_STATES)));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kArcCpuCgroupFail, error->GetCode());
#else
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.SetArcCpuRestriction(
      &error, static_cast<uint32_t>(CONTAINER_CPU_RESTRICTION_BACKGROUND)));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kNotAvailable, error->GetCode());
#endif
}

TEST_F(SessionManagerImplTest, EmitArcBooted) {
#if USE_CHEETS
  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcBootedSignal),
                              ElementsAre(StartsWith("ANDROID_DATA_OLD_DIR="))))
      .Times(1);
  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.EmitArcBooted(&error, kSaneEmail));
    EXPECT_FALSE(error.get());
  }

  EXPECT_CALL(upstart_signal_emitter_delegate_,
              OnSignalEmitted(StrEq(SessionManagerImpl::kArcBootedSignal),
                              ElementsAre()))
      .Times(1);
  {
    brillo::ErrorPtr error;
    EXPECT_TRUE(impl_.EmitArcBooted(&error, std::string()));
    EXPECT_FALSE(error.get());
  }
#else
  brillo::ErrorPtr error;
  EXPECT_FALSE(impl_.EmitArcBooted(&error, kSaneEmail));
  ASSERT_TRUE(error.get());
  EXPECT_EQ(dbus_error::kNotAvailable, error->GetCode());
#endif
}

class SessionManagerImplStaticTest : public ::testing::Test {
 public:
  SessionManagerImplStaticTest() {}
  virtual ~SessionManagerImplStaticTest() {}

  bool ValidateEmail(const string& email_address) {
    return SessionManagerImpl::ValidateEmail(email_address);
  }

  bool ValidateAccountIdKey(const string& account_id) {
    return SessionManagerImpl::ValidateAccountIdKey(account_id);
  }
};

TEST_F(SessionManagerImplStaticTest, EmailAddressTest) {
  EXPECT_TRUE(ValidateEmail("user_who+we.like@some-where.com"));
  EXPECT_TRUE(ValidateEmail("john_doe's_mail@some-where.com"));
}

TEST_F(SessionManagerImplStaticTest, EmailAddressNonAsciiTest) {
  char invalid[4] = "a@m";
  invalid[2] = static_cast<char>(254);
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

TEST_F(SessionManagerImplStaticTest, AccountIdKeyTest) {
  EXPECT_TRUE(ValidateAccountIdKey("g-1234567890123456"));
  // email string is invalid GaiaIdKey
  EXPECT_FALSE(ValidateAccountIdKey("john@some.where.com"));
  // Only alphanumeric characters plus a colon are allowed.
  EXPECT_TRUE(ValidateAccountIdKey("g-1234567890"));
  EXPECT_TRUE(ValidateAccountIdKey("g-abcdef0123456789"));
  EXPECT_TRUE(ValidateAccountIdKey("g-ABCDEF0123456789"));
  EXPECT_FALSE(ValidateAccountIdKey("g-123@some.where.com"));
  EXPECT_FALSE(ValidateAccountIdKey("g-123@localhost"));
  // Active Directory account keys.
  EXPECT_TRUE(ValidateAccountIdKey("a-abcdef0123456789"));
  EXPECT_FALSE(ValidateAccountIdKey("a-123@localhost"));
}

}  // namespace login_manager
