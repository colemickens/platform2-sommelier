// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/stringprintf.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <dbus/bus.h>
#include <dbus/login_manager/dbus-constants.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "authpolicy/path_service.h"
#include "authpolicy/policy/policy_encoder_helper.h"
#include "authpolicy/policy/preg_policy_writer.h"
#include "authpolicy/proto_bindings/active_directory_info.pb.h"
#include "authpolicy/samba_interface.h"
#include "authpolicy/stub_common.h"
#include "bindings/chrome_device_policy.pb.h"
#include "bindings/cloud_policy.pb.h"
#include "bindings/device_management_backend.pb.h"
#include "bindings/policy_constants.h"

using brillo::dbus_utils::DBusObject;
using brillo::dbus_utils::ExtractMethodCallResults;
using dbus::MessageWriter;
using dbus::MockBus;
using dbus::MockExportedObject;
using dbus::MockObjectProxy;
using dbus::ObjectPath;
using dbus::ObjectProxy;
using dbus::Response;
using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::SaveArg;

namespace em = enterprise_management;

namespace authpolicy {
namespace {

// Some arbitrary D-Bus message serial number. Required for mocking D-Bus calls.
const int kDBusSerial = 123;

// Some constants for policy testing.
const bool kPolicyBool = true;
const int kPolicyInt = 321;
const bool kOtherPolicyBool = false;
const int kOtherPolicyInt = 234;

const char kHomepageUrl[] = "www.example.com";
const char kTimezone[] = "Sankt Aldegund Central Time";
const char kAltTimezone[] = "Alf Fabrik Central Time";

// Checks and casts an integer |error| to the corresponding ErrorType.
ErrorType CastError(int error) {
  EXPECT_GE(error, 0);
  EXPECT_LT(error, ERROR_COUNT);
  return static_cast<ErrorType>(error);
}

// Create a file descriptor pointing to a pipe that contains the given data.
dbus::FileDescriptor MakeFileDescriptor(const char* data) {
  int fds[2];
  EXPECT_TRUE(base::CreateLocalNonBlockingPipe(fds));
  dbus::FileDescriptor read_dbus_fd;
  read_dbus_fd.PutValue(fds[0]);
  read_dbus_fd.CheckValidity();
  base::ScopedFD write_scoped_fd(fds[1]);
  EXPECT_TRUE(
      base::WriteFileDescriptor(write_scoped_fd.get(), data, strlen(data)));
  return read_dbus_fd;
}

// Shortcut to create a file descriptor from a valid password (valid in the
// sense that the stub executables won't trigger any error behavior).
dbus::FileDescriptor MakePasswordFd() {
  return MakeFileDescriptor(kPassword);
}

// Stub completion callback for RegisterAsync().
void DoNothing(bool /* unused */) {}

// Helper class that points some paths to convenient locations we can write to.
class TestPathService : public PathService {
 public:
  explicit TestPathService(const base::FilePath& base_path)
      : PathService(false) {
    // Stub binaries are in the OUT folder politely provided by the test script.
    base::FilePath stub_path(getenv("OUT"));
    CHECK(!stub_path.empty());

    // Override paths.
    Insert(Path::TEMP_DIR, base_path.Append("temp").value());
    Insert(Path::STATE_DIR, base_path.Append("state").value());
    Insert(Path::KINIT, stub_path.Append("stub_kinit").value());
    Insert(Path::KLIST, stub_path.Append("stub_klist").value());
    Insert(Path::NET, stub_path.Append("stub_net").value());
    Insert(Path::SMBCLIENT, stub_path.Append("stub_smbclient").value());

    // Fill in the rest of the paths and build dependend paths.
    Initialize();
  }
};

// Metrics library that eats in particular timer errors.
class TestMetricsLibrary : public MetricsLibrary {
 public:
  bool SendToUMA(const std::string&, int, int, int, int) override {
    return true;
  }
};

// Version of AuthPolicyMetrics that just counts stats.
class TestMetrics : public AuthPolicyMetrics {
 public:
  TestMetrics() {
    // Prevent some error messages from timers.
    test_metrics_.Init();
    chromeos_metrics::TimerReporter::set_metrics_lib(&test_metrics_);
  }

  // Prints out a list of untested metrics activity. This makes sure that no
  // metrics are reported that are not expected by the tests.
  ~TestMetrics() override {
    chromeos_metrics::TimerReporter::set_metrics_lib(nullptr);

    std::map<MetricType, const char*> metrics_str;
    metrics_str[METRIC_KINIT_FAILED_TRY_COUNT] =
        "METRIC_KINIT_FAILED_TRY_COUNT";
    metrics_str[METRIC_SMBCLIENT_FAILED_TRY_COUNT] =
        "METRIC_SMBCLIENT_FAILED_TRY_COUNT";
    metrics_str[METRIC_DOWNLOAD_GPO_COUNT] = "METRIC_DOWNLOAD_GPO_COUNT";
    CHECK_EQ(METRIC_COUNT, metrics_str.size());
    static_assert(METRIC_COUNT == 3, "Add new values!");

    std::map<DBusCallType, const char*> dbus_str;
    dbus_str[DBUS_CALL_AUTHENTICATE_USER] = "DBUS_CALL_AUTHENTICATE_USER";
    dbus_str[DBUS_CALL_GET_USER_STATUS] = "DBUS_CALL_GET_USER_STATUS";
    dbus_str[DBUS_CALL_JOIN_AD_DOMAIN] = "DBUS_CALL_JOIN_AD_DOMAIN";
    dbus_str[DBUS_CALL_REFRESH_USER_POLICY] = "DBUS_CALL_REFRESH_USER_POLICY";
    dbus_str[DBUS_CALL_REFRESH_DEVICE_POLICY] =
        "DBUS_CALL_REFRESH_DEVICE_POLICY";
    CHECK_EQ(DBUS_CALL_COUNT, dbus_str.size());
    static_assert(DBUS_CALL_COUNT == 5, "Add new values!");

    std::string log_str;
    for (const auto& kv : metrics_report_count_) {
      log_str += base::StringPrintf(
          "\n  EXPECT_EQ(%i, metrics_->GetMetricReportCount(%s));",
          kv.second,
          metrics_str[kv.first]);
    }
    for (const auto& kv : dbus_report_count_) {
      log_str += base::StringPrintf(
          "\n  EXPECT_EQ(%i, metrics_->GetDBusReportCount(%s));",
          kv.second,
          dbus_str[kv.first]);
    }
    EXPECT_TRUE(log_str.empty()) << "Unexpected metrics activity. "
                                 << "If this looks right, add " << log_str;
  }

  void Report(MetricType metric_type, int sample) override {
    last_metrics_sample_[metric_type] = sample;
    metrics_report_count_[metric_type]++;
  }

  void ReportDBusResult(DBusCallType call_type,
                        ErrorType /* error */) override {
    dbus_report_count_[call_type]++;
  }

  // Returns the most recently reported sample for the given |metric_type| or
  // -1 if the metric has not been reported.
  int GetLastMetricSample(MetricType metric_type) {
    auto iter = last_metrics_sample_.find(metric_type);
    return iter != last_metrics_sample_.end() ? iter->second : -1;
  }

  // Returns how often Report() was called with given |metric_type| and erases
  // the count. Inefficient if metric_type isn't in the map, but shorter :)
  int GetMetricReportCount(MetricType metric_type) {
    const int count = metrics_report_count_[metric_type];
    metrics_report_count_.erase(metric_type);
    return count;
  }

  // Returns how often ReportDBusResult() was called with given |call_type| and
  // erases the count. Inefficient if call_type isn't in the map, but shorter :)
  int GetDBusReportCount(DBusCallType call_type) {
    const int count = dbus_report_count_[call_type];
    dbus_report_count_.erase(call_type);
    return count;
  }

 private:
  TestMetricsLibrary test_metrics_;
  std::map<MetricType, int> last_metrics_sample_;
  std::map<MetricType, int> metrics_report_count_;
  std::map<DBusCallType, int> dbus_report_count_;
};

// Helper to check the ErrorType value returned by authpolicy D-Bus calls.
// |was_called| is a marker used by the code that queues this callback to make
// sure that this callback was indeed called.
void CheckError(ErrorType expected_error,
                bool* was_called,
                std::unique_ptr<Response> response) {
  EXPECT_TRUE(response.get());
  dbus::MessageReader reader(response.get());
  int32_t int_error;
  EXPECT_TRUE(reader.PopInt32(&int_error));
  ErrorType actual_error = CastError(int_error);
  EXPECT_EQ(expected_error, actual_error);
  EXPECT_TRUE(was_called);
  EXPECT_FALSE(*was_called);
  *was_called = true;
}

}  // namespace

// Integration test for the authpolicyd D-Bus interface.
//
// Since the Active Directory protocols are a black box to us, a stub local
// server cannot be used. Instead, the Samba/Kerberos binaries are stubbed out.
//
// Error behavior is triggered by passing special user principals or passwords
// to the stub binaries. For instance, using |kNonExistingUserPrincipal| makes
// stub_kinit behave as if the requested account does not exist on the server.
// The same principle is used throughout this test.
//
// During policy fetch, authpolicy sends D-Bus messages to Session Manager. This
// communication is mocked out.
class AuthPolicyTest : public testing::Test {
 public:
  void SetUp() override {
    // The message loop registers a task runner with the current thread, which
    // is used by TgtManager to post automatic TGT renewal tasks.
    message_loop_ = base::MakeUnique<base::MessageLoop>();

    const ObjectPath object_path(std::string("/object/path"));
    auto dbus_object =
        base::MakeUnique<DBusObject>(nullptr, mock_bus_, object_path);

    metrics_ = base::MakeUnique<TestMetrics>();

    // Create path service with all paths pointing into a temp directory.
    CHECK(base::CreateNewTempDirectory("" /* prefix (ignored) */, &base_path_));
    paths_ = base::MakeUnique<TestPathService>(base_path_);

    // Create the state directory since authpolicyd assumes its existence.
    const base::FilePath state_path =
        base::FilePath(paths_->Get(Path::STATE_DIR));
    CHECK(base::CreateDirectory(state_path));

    // Set stub preg path. Since it is not trivial to pass the full path to the
    // stub binaries, we simply use the directory from the krb5.conf file.
    const base::FilePath gpo_dir =
        base::FilePath(paths_->Get(Path::USER_KRB5_CONF)).DirName();
    DCHECK(gpo_dir ==
           base::FilePath(paths_->Get(Path::DEVICE_KRB5_CONF)).DirName());
    stub_gpo1_path_ = gpo_dir.Append(kGpo1Filename);
    stub_gpo2_path_ = gpo_dir.Append(kGpo2Filename);

    // Mock out D-Bus initialization.
    mock_exported_object_ =
        new MockExportedObject(mock_bus_.get(), object_path);
    EXPECT_CALL(*mock_bus_, GetExportedObject(object_path))
        .Times(1)
        .WillOnce(Return(mock_exported_object_.get()));
    EXPECT_CALL(*mock_bus_, GetDBusTaskRunner())
        .Times(1)
        .WillOnce(Return(message_loop_->task_runner().get()));
    EXPECT_CALL(*mock_exported_object_.get(), ExportMethod(_, _, _, _))
        .Times(8);

    // Create AuthPolicy instance.
    authpolicy_ = base::MakeUnique<AuthPolicy>(metrics_.get(), paths_.get());
    EXPECT_EQ(ERROR_NONE, authpolicy_->Initialize(false /* expect_config */));

    // Don't sleep for kinit/smbclient retries, it just prolongs our tests.
    authpolicy_->DisableRetrySleepForTesting();

    // Set up mock object proxy for session manager called from authpolicy.
    mock_session_manager_proxy_ = new MockObjectProxy(
        mock_bus_.get(),
        login_manager::kSessionManagerServiceName,
        dbus::ObjectPath(login_manager::kSessionManagerServicePath));
    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(login_manager::kSessionManagerServiceName,
                               dbus::ObjectPath(
                                   login_manager::kSessionManagerServicePath)))
        .WillOnce(Return(mock_session_manager_proxy_.get()));
    EXPECT_CALL(*mock_session_manager_proxy_.get(), CallMethod(_, _, _))
        .WillRepeatedly(
            Invoke(this, &AuthPolicyTest::StubCallStorePolicyMethod));

    authpolicy_->RegisterAsync(std::move(dbus_object), base::Bind(&DoNothing));
  }

  // Stub method called by the Session Manager mock to store policy. Validates
  // the type of policy (user/device) contained in the |method_call|. If set by
  // the individual unit tests, calls |validate_user_policy_| or
  // |validate_device_policy_|  to validate the contents of the policy proto.
  void StubCallStorePolicyMethod(dbus::MethodCall* method_call,
                                 int /* timeout_ms */,
                                 ObjectProxy::ResponseCallback callback) {
    // Safety check to make sure that old values are not carried along.
    EXPECT_FALSE(store_policy_called_);
    EXPECT_FALSE(validate_user_policy_called_);
    EXPECT_FALSE(validate_device_policy_called_);
    store_policy_called_ = true;

    // Based on the method name, check whether this is user or device policy.
    EXPECT_TRUE(method_call);
    EXPECT_TRUE(method_call->GetMember() ==
                    login_manager::kSessionManagerStoreUnsignedPolicy ||
                method_call->GetMember() ==
                    login_manager::kSessionManagerStoreUnsignedPolicyForUser);
    bool is_user_policy =
        method_call->GetMember() ==
        login_manager::kSessionManagerStoreUnsignedPolicyForUser;

    // Extract the policy blob from the method call.
    std::string account_id_key;
    std::vector<uint8_t> response_blob;
    brillo::ErrorPtr error;
    if (is_user_policy) {
      EXPECT_TRUE(ExtractMethodCallResults(
          method_call, &error, &account_id_key, &response_blob));
    } else {
      EXPECT_TRUE(
          ExtractMethodCallResults(method_call, &error, &response_blob));
    }

    // Unwrap the three gazillion layers or policy.
    const std::string response_blob_str(response_blob.begin(),
                                        response_blob.end());
    em::PolicyFetchResponse policy_response;
    EXPECT_TRUE(policy_response.ParseFromString(response_blob_str));
    em::PolicyData policy_data;
    EXPECT_TRUE(policy_data.ParseFromString(policy_response.policy_data()));
    const char* const expected_policy_type =
        is_user_policy ? kChromeUserPolicyType : kChromeDevicePolicyType;
    EXPECT_EQ(expected_policy_type, policy_data.policy_type());

    if (is_user_policy) {
      em::CloudPolicySettings policy;
      EXPECT_TRUE(policy.ParseFromString(policy_data.policy_value()));
      if (validate_user_policy_) {
        validate_user_policy_(policy);
        validate_user_policy_called_ = true;
      }
    } else {
      em::ChromeDeviceSettingsProto policy;
      EXPECT_TRUE(policy.ParseFromString(policy_data.policy_value()));
      if (validate_device_policy_) {
        validate_device_policy_(policy);
        validate_device_policy_called_ = true;
      }
    }

    // Answer authpolicy with an empty response to signal that policy has been
    // stored.
    EXPECT_FALSE(callback.is_null());
    callback.Run(Response::CreateEmpty().get());
  }

  void TearDown() override {
    EXPECT_EQ(expected_dbus_calls[DBUS_CALL_AUTHENTICATE_USER],
              metrics_->GetDBusReportCount(DBUS_CALL_AUTHENTICATE_USER));
    EXPECT_EQ(expected_dbus_calls[DBUS_CALL_GET_USER_STATUS],
              metrics_->GetDBusReportCount(DBUS_CALL_GET_USER_STATUS));
    EXPECT_EQ(expected_dbus_calls[DBUS_CALL_JOIN_AD_DOMAIN],
              metrics_->GetDBusReportCount(DBUS_CALL_JOIN_AD_DOMAIN));
    EXPECT_EQ(expected_dbus_calls[DBUS_CALL_REFRESH_USER_POLICY],
              metrics_->GetDBusReportCount(DBUS_CALL_REFRESH_USER_POLICY));
    EXPECT_EQ(expected_dbus_calls[DBUS_CALL_REFRESH_DEVICE_POLICY],
              metrics_->GetDBusReportCount(DBUS_CALL_REFRESH_DEVICE_POLICY));

    EXPECT_CALL(*mock_exported_object_, Unregister()).Times(1);
    // Don't not leave no mess behind.
    base::DeleteFile(base_path_, true /* recursive */);
  }

 protected:
  // Joins a (stub) Active Directory domain. Returns the error code.
  ErrorType Join(const std::string& machine_name,
                 const std::string& user_principal,
                 dbus::FileDescriptor password_fd) {
    expected_dbus_calls[DBUS_CALL_JOIN_AD_DOMAIN]++;
    return CastError(
        authpolicy_->JoinADDomain(machine_name, user_principal, password_fd));
  }

  // Authenticates to a (stub) Active Directory domain with the given
  // credentials and returns the error code. Assigns the user account info to
  // |account_info| if a non-nullptr is provided.
  ErrorType Auth(
      const std::string& user_principal,
      const std::string& account_id,
      dbus::FileDescriptor password_fd,
      authpolicy::ActiveDirectoryAccountInfo* account_info = nullptr) {
    int32_t error = ERROR_NONE;
    std::vector<uint8_t> account_info_blob;
    expected_dbus_calls[DBUS_CALL_AUTHENTICATE_USER]++;
    authpolicy_->AuthenticateUser(
        user_principal, account_id, password_fd, &error, &account_info_blob);
    if (error == ERROR_NONE) {
      EXPECT_FALSE(account_info_blob.empty());
      if (account_info) {
        EXPECT_TRUE(account_info->ParseFromArray(
            account_info_blob.data(),
            static_cast<int>(account_info_blob.size())));
      }
    } else {
      EXPECT_TRUE(account_info_blob.empty());
    }
    return CastError(error);
  }

  // Gets a fake user status from a (stub) Active Directory service.
  // |account_id| is the id (aka objectGUID) of the user. Assigns the user's
  // status to |user_status| if a non-nullptr is given.
  ErrorType GetUserStatus(
      const std::string& account_id,
      authpolicy::ActiveDirectoryUserStatus* user_status = nullptr) {
    int32_t error = ERROR_NONE;
    std::vector<uint8_t> user_status_blob;
    expected_dbus_calls[DBUS_CALL_GET_USER_STATUS]++;
    authpolicy_->GetUserStatus(account_id, &error, &user_status_blob);
    if (error == ERROR_NONE) {
      EXPECT_FALSE(user_status_blob.empty());
      if (user_status) {
        EXPECT_TRUE(user_status->ParseFromArray(
            user_status_blob.data(),
            static_cast<int>(user_status_blob.size())));
      }
    } else {
      EXPECT_TRUE(user_status_blob.empty());
    }
    return CastError(error);
  }

  // Authenticates to a (stub) Active Directory domain with default credentials.
  // Returns the account id key.
  std::string DefaultAuth() {
    authpolicy::ActiveDirectoryAccountInfo account_info;
    EXPECT_EQ(ERROR_NONE,
              Auth(kUserPrincipal, "", MakePasswordFd(), &account_info));
    return kActiveDirectoryPrefix + account_info.account_id();
  }

  // Calls AuthPolicy::RefreshUserPolicy(). Verifies that
  // StubCallStorePolicyMethod() and validate_user_policy_ are called as
  // expected. These callbacks verify that the policy protobuf is valid and
  // validate the contents.
  void FetchAndValidateUserPolicy(const std::string& account_id_key,
                                  ErrorType expected_error) {
    dbus::MethodCall method_call(kAuthPolicyInterface,
                                 kAuthPolicyRefreshUserPolicy);
    method_call.SetSerial(kDBusSerial);
    store_policy_called_ = false;
    validate_user_policy_called_ = false;
    validate_device_policy_called_ = false;
    bool callback_was_called = false;
    AuthPolicy::PolicyResponseCallback callback =
        base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
            &method_call,
            base::Bind(&CheckError, expected_error, &callback_was_called));
    expected_dbus_calls[DBUS_CALL_REFRESH_USER_POLICY]++;
    authpolicy_->RefreshUserPolicy(std::move(callback), account_id_key);

    // If policy fetch succeeds, authpolicy_ makes a D-Bus call to Session
    // Manager to store policy. We intercept this call and point it to
    // StubCallStorePolicyMethod(), which validates policy and calls CheckError.
    // If policy fetch fails, StubCallStorePolicyMethod() is not called, but
    // authpolicy calls CheckError directly.
    EXPECT_EQ(expected_error == ERROR_NONE, store_policy_called_);
    EXPECT_EQ(expected_error == ERROR_NONE, validate_user_policy_called_);
    EXPECT_FALSE(validate_device_policy_called_);
    EXPECT_TRUE(callback_was_called);  // Make sure CheckError() was called.
  }

  // Calls AuthPolicy::RefreshDevicePolicy(). Verifies that
  // StubCallStorePolicyMethod() and validate_device_policy_ are called as
  // expected. These callbacks verify that the policy protobuf is valid and
  // validate the contents.
  void FetchAndValidateDevicePolicy(ErrorType expected_error) {
    dbus::MethodCall method_call(kAuthPolicyInterface,
                                 kAuthPolicyRefreshDevicePolicy);
    method_call.SetSerial(kDBusSerial);
    store_policy_called_ = false;
    validate_user_policy_called_ = false;
    validate_device_policy_called_ = false;
    bool callback_was_called = false;
    AuthPolicy::PolicyResponseCallback callback =
        base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
            &method_call,
            base::Bind(&CheckError, expected_error, &callback_was_called));
    expected_dbus_calls[DBUS_CALL_REFRESH_DEVICE_POLICY]++;
    authpolicy_->RefreshDevicePolicy(std::move(callback));

    // If policy fetch succeeds, authpolicy_ makes a D-Bus call to Session
    // Manager to store policy. We intercept this call and point it to
    // StubCallStorePolicyMethod(), which validates policy and calls CheckError.
    // If policy fetch fails, StubCallStorePolicyMethod() is not called, but
    // authpolicy calls CheckError directly.
    EXPECT_EQ(expected_error == ERROR_NONE, store_policy_called_);
    EXPECT_EQ(expected_error == ERROR_NONE, validate_device_policy_called_);
    EXPECT_FALSE(validate_user_policy_called_);
    EXPECT_TRUE(callback_was_called);  // Make sure CheckError() was called.
  }

  // Checks whether the user |policy| is empty.
  static void CheckUserPolicyEmpty(const em::CloudPolicySettings& policy) {
    em::CloudPolicySettings empty_policy;
    EXPECT_EQ(policy.ByteSize(), empty_policy.ByteSize());
  }

  // Checks whether the device |policy| is empty.
  static void CheckDevicePolicyEmpty(
      const em::ChromeDeviceSettingsProto& policy) {
    em::ChromeDeviceSettingsProto empty_policy;
    EXPECT_EQ(policy.ByteSize(), empty_policy.ByteSize());
  }

  // Authpolicyd revokes write permissions on config.dat. Some tests perform two
  // domain joins, though, and need to overwrite the previously generated config
  // file.
  bool MakeConfigWriteable() {
    const base::FilePath config_path(paths_->Get(Path::CONFIG_DAT));
    const int mode = base::FILE_PERMISSION_READ_BY_USER |
                     base::FILE_PERMISSION_WRITE_BY_USER;
    return base::SetPosixFilePermissions(config_path, mode);
  }

  std::unique_ptr<base::MessageLoop> message_loop_;

  scoped_refptr<MockBus> mock_bus_ = new MockBus(dbus::Bus::Options());
  scoped_refptr<MockExportedObject> mock_exported_object_;
  scoped_refptr<MockObjectProxy> mock_session_manager_proxy_;

  // Keep this order! auth_policy_ must be last as it depends on the other two.
  std::unique_ptr<TestMetrics> metrics_;
  std::unique_ptr<TestPathService> paths_;
  std::unique_ptr<AuthPolicy> authpolicy_;

  base::FilePath base_path_;
  base::FilePath stub_gpo1_path_;
  base::FilePath stub_gpo2_path_;

  // Markers to check whether various callbacks are actually called.
  bool store_policy_called_ = false;            // StubCallStorePolicyMethod().
  bool validate_user_policy_called_ = false;    // Policy validation
  bool validate_device_policy_called_ = false;  //   callbacks below.

  // Must be set in unit tests to validate policy protos which authpolicy_ sends
  // to Session Manager via D-Bus (resp. to StubCallStorePolicyMethod() in these
  // tests).
  std::function<void(const em::CloudPolicySettings&)> validate_user_policy_;
  std::function<void(const em::ChromeDeviceSettingsProto&)>
      validate_device_policy_;

 private:
  // Expected calls of metrics reporting functions, set and checked internally.
  std::map<DBusCallType, int> expected_dbus_calls;
};

// Can't fetch user policy if the user is not logged in.
TEST_F(AuthPolicyTest, UserPolicyFailsNotLoggedIn) {
  FetchAndValidateUserPolicy("account_id_key", ERROR_NOT_LOGGED_IN);
}

// Can't fetch device policy if the device is not joined.
TEST_F(AuthPolicyTest, DevicePolicyFailsNotJoined) {
  FetchAndValidateDevicePolicy(ERROR_NOT_JOINED);
}

// Authentication fails if the machine is not joined.
TEST_F(AuthPolicyTest, AuthFailsNotJoined) {
  EXPECT_EQ(ERROR_NOT_JOINED, Auth(kUserPrincipal, "", MakePasswordFd()));
}

// Successful domain join.
TEST_F(AuthPolicyTest, JoinSucceeds) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
}

// Successful user authentication.
TEST_F(AuthPolicyTest, AuthSucceeds) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, Auth(kUserPrincipal, "", MakePasswordFd()));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Successful user authentication with given account id.
TEST_F(AuthPolicyTest, AuthSucceedsWithKnownAccountId) {
  authpolicy::ActiveDirectoryAccountInfo account_info;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE,
            Auth(kUserPrincipal, kAccountId, MakePasswordFd(), &account_info));
  EXPECT_EQ(kAccountId, account_info.account_id());
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// User authentication fails with bad (non-existent) account id.
TEST_F(AuthPolicyTest, AuthFailsWithBadAccountId) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_BAD_USER_NAME,
            Auth(kUserPrincipal, kBadAccountId, MakePasswordFd()));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Successful user authentication.
TEST_F(AuthPolicyTest, AuthSetsAccountInfo) {
  authpolicy::ActiveDirectoryAccountInfo account_info;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE,
            Auth(kUserPrincipal, "", MakePasswordFd(), &account_info));
  EXPECT_EQ(kAccountId, account_info.account_id());
  EXPECT_EQ(kDisplayName, account_info.display_name());
  EXPECT_EQ(kGivenName, account_info.given_name());
  EXPECT_EQ(kUserName, account_info.sam_account_name());
  EXPECT_EQ(kPwdLastSet, account_info.pwd_last_set());
  EXPECT_EQ(kUserAccountControl, account_info.user_account_control());
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Authentication fails for badly formatted user principal name.
TEST_F(AuthPolicyTest, AuthFailsInvalidUpn) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_PARSE_UPN_FAILED,
            Auth(kInvalidUserPrincipal, "", MakePasswordFd()));
}

// Authentication fails for non-existing user principal name.
TEST_F(AuthPolicyTest, AuthFailsBadUpn) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_BAD_USER_NAME,
            Auth(kNonExistingUserPrincipal, "", MakePasswordFd()));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Authentication fails for wrong password.
TEST_F(AuthPolicyTest, AuthFailsBadPassword) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            Auth(kUserPrincipal, "", MakeFileDescriptor(kWrongPassword)));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Authentication fails for expired password.
TEST_F(AuthPolicyTest, AuthFailsExpiredPassword) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_PASSWORD_EXPIRED,
            Auth(kUserPrincipal, "", MakeFileDescriptor(kExpiredPassword)));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Authentication fails if there's a network issue.
TEST_F(AuthPolicyTest, AuthFailsNetworkProblem) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NETWORK_PROBLEM,
            Auth(kNetworkErrorUserPrincipal, "", MakePasswordFd()));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Authentication retries without KDC if it fails the first time.
TEST_F(AuthPolicyTest, AuthSucceedsKdcRetry) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, Auth(kKdcRetryUserPrincipal, "", MakePasswordFd()));
  EXPECT_EQ(3, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Can't get user status before domain join.
TEST_F(AuthPolicyTest, GetUserStatusFailsNotJoined) {
  EXPECT_EQ(ERROR_NOT_JOINED, GetUserStatus(kAccountId));
}

// GetUserStatus fails with bad account id.
TEST_F(AuthPolicyTest, GetUserStatusFailsBadAccountId) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_BAD_USER_NAME, GetUserStatus(kBadAccountId));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// GetUserStatus succeeds without auth, but tgt is invalid.
TEST_F(AuthPolicyTest, GetUserStatusSucceedsTgtNotFound) {
  ActiveDirectoryUserStatus status;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, GetUserStatus(kAccountId, &status));
  EXPECT_EQ(ActiveDirectoryUserStatus::TGT_NOT_FOUND, status.tgt_status());
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// GetUserStatus succeeds with join and auth, but with an expired TGT.
TEST_F(AuthPolicyTest, GetUserStatusSucceedsTgtExpired) {
  ActiveDirectoryUserStatus status;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, Auth(kExpiredTgtUserPrincipal, "", MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, GetUserStatus(kAccountId, &status));
  EXPECT_EQ(ActiveDirectoryUserStatus::TGT_EXPIRED, status.tgt_status());
  EXPECT_EQ(3, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// GetUserStatus succeeds with join and auth.
TEST_F(AuthPolicyTest, GetUserStatusSucceeds) {
  ActiveDirectoryUserStatus status;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, Auth(kUserPrincipal, "", MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, GetUserStatus(kAccountId, &status));

  ActiveDirectoryUserStatus expected_status;
  ActiveDirectoryAccountInfo& expected_account_info =
      *expected_status.mutable_account_info();
  expected_account_info.set_account_id(kAccountId);
  expected_account_info.set_display_name(kDisplayName);
  expected_account_info.set_given_name(kGivenName);
  expected_account_info.set_sam_account_name(kUserName);
  expected_account_info.set_pwd_last_set(kPwdLastSet);
  expected_account_info.set_user_account_control(kUserAccountControl);
  expected_status.set_tgt_status(ActiveDirectoryUserStatus::TGT_VALID);
  expected_status.set_password_status(
      ActiveDirectoryUserStatus::PASSWORD_VALID);
  expected_status.set_last_auth_error(ERROR_NONE);

  // Note that protobuf equality comparison is not supported.
  std::string status_blob, expected_status_blob;
  EXPECT_TRUE(status.SerializeToString(&status_blob));
  EXPECT_TRUE(expected_status.SerializeToString(&expected_status_blob));
  EXPECT_EQ(expected_status_blob, status_blob);

  EXPECT_EQ(3, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// GetUserStatus actually contains the last auth error.
TEST_F(AuthPolicyTest, GetUserStatusReportsLastAuthError) {
  ActiveDirectoryUserStatus status;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_PASSWORD_EXPIRED,
            Auth(kUserPrincipal, "", MakeFileDescriptor(kExpiredPassword)));
  EXPECT_EQ(ERROR_NONE, GetUserStatus(kAccountId, &status));
  EXPECT_EQ(ERROR_PASSWORD_EXPIRED, status.last_auth_error());
  EXPECT_EQ(3, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// GetUserStatus reports to expire the password.
TEST_F(AuthPolicyTest, GetUserStatusReportsExpiredPasswords) {
  ActiveDirectoryUserStatus status;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE,
            Auth(kUserPrincipal, kExpiredPasswordAccountId, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, GetUserStatus(kExpiredPasswordAccountId, &status));
  EXPECT_EQ(ActiveDirectoryUserStatus::PASSWORD_EXPIRED,
            status.password_status());
  EXPECT_EQ(3, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// GetUserStatus does not report expired passwords if UF_DONT_EXPIRE_PASSWD is
// set.
TEST_F(AuthPolicyTest, GetUserStatusDontReportNeverExpirePasswords) {
  ActiveDirectoryUserStatus status;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(
      ERROR_NONE,
      Auth(kUserPrincipal, kNeverExpirePasswordAccountId, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, GetUserStatus(kNeverExpirePasswordAccountId, &status));
  EXPECT_EQ(ActiveDirectoryUserStatus::PASSWORD_VALID,
            status.password_status());
  EXPECT_EQ(3, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// GetUserStatus reports password changes.
TEST_F(AuthPolicyTest, GetUserStatusReportChangedPasswords) {
  ActiveDirectoryUserStatus status;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE,
            Auth(kPasswordChangedUserPrincipal, "", MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, GetUserStatus(kPasswordChangedAccountId, &status));
  EXPECT_EQ(ActiveDirectoryUserStatus::PASSWORD_CHANGED,
            status.password_status());
  EXPECT_EQ(3, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Join fails if there's a network issue.
TEST_F(AuthPolicyTest, JoinFailsNetworkProblem) {
  EXPECT_EQ(ERROR_NETWORK_PROBLEM,
            Join(kMachineName, kNetworkErrorUserPrincipal, MakePasswordFd()));
}

// Join fails for badly formatted user principal name.
TEST_F(AuthPolicyTest, JoinFailsInvalidUpn) {
  EXPECT_EQ(ERROR_PARSE_UPN_FAILED,
            Join(kMachineName, kInvalidUserPrincipal, MakePasswordFd()));
}

// Join fails for non-existing user principal name, but the error message is the
// same as for wrong password.
TEST_F(AuthPolicyTest, JoinFailsBadUpn) {
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            Join(kMachineName, kNonExistingUserPrincipal, MakePasswordFd()));
}

// Join fails for wrong password.
TEST_F(AuthPolicyTest, JoinFailsBadPassword) {
  EXPECT_EQ(
      ERROR_BAD_PASSWORD,
      Join(kMachineName, kUserPrincipal, MakeFileDescriptor(kWrongPassword)));
}

// Join fails if user can't join a machine to the domain.
TEST_F(AuthPolicyTest, JoinFailsAccessDenied) {
  EXPECT_EQ(ERROR_JOIN_ACCESS_DENIED,
            Join(kMachineName, kAccessDeniedUserPrincipal, MakePasswordFd()));
}

// Join fails if the machine name is too long.
TEST_F(AuthPolicyTest, JoinFailsMachineNameTooLong) {
  EXPECT_EQ(ERROR_MACHINE_NAME_TOO_LONG,
            Join(kTooLongMachineName, kUserPrincipal, MakePasswordFd()));
}

// Join fails if the machine name contains invalid characters.
TEST_F(AuthPolicyTest, JoinFailsInvalidMachineName) {
  EXPECT_EQ(ERROR_INVALID_MACHINE_NAME,
            Join(kInvalidMachineName, kUserPrincipal, MakePasswordFd()));
}

// Join fails if the user can't join additional machines.
TEST_F(AuthPolicyTest, JoinFailsInsufficientQuota) {
  EXPECT_EQ(
      ERROR_USER_HIT_JOIN_QUOTA,
      Join(kMachineName, kInsufficientQuotaUserPrincipal, MakePasswordFd()));
}

// Successful user policy fetch with empty policy.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceeds) {
  validate_user_policy_ = &CheckUserPolicyEmpty;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Successful user policy fetch with actual data.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceedsWithData) {
  // Write a preg file with all basic data types. The file is picked up by
  // stub_net and "downloaded" by stub_smbclient.
  policy::PRegPolicyWriter writer(policy::GetRegistryKey());
  writer.AppendBoolean(policy::key::kSearchSuggestEnabled, kPolicyBool);
  writer.AppendInteger(policy::key::kPolicyRefreshRate, kPolicyInt);
  writer.AppendString(policy::key::kHomepageLocation, kHomepageUrl);
  const std::vector<std::string> apps = {"App1", "App2"};
  writer.AppendStringList(policy::key::kPinnedLauncherApps, apps);
  writer.WriteToFile(stub_gpo1_path_);

  // Validate that the protobufs sent from authpolicy to Session Manager
  // actually contain the policies set above. This validator is called by
  // FetchAndValidateUserPolicy below.
  validate_user_policy_ = [apps](const em::CloudPolicySettings& policy) {
    EXPECT_EQ(kPolicyBool, policy.searchsuggestenabled().value());
    EXPECT_EQ(kPolicyInt, policy.policyrefreshrate().value());
    EXPECT_EQ(kHomepageUrl, policy.homepagelocation().value());
    const em::StringList& apps_proto = policy.pinnedlauncherapps().value();
    EXPECT_EQ(apps_proto.entries_size(), static_cast<int>(apps.size()));
    for (int n = 0; n < apps_proto.entries_size(); ++n)
      EXPECT_EQ(apps_proto.entries(n), apps.at(n));
  };
  EXPECT_EQ(ERROR_NONE,
            Join(kOneGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Verify that PolicyLevel is encoded properly.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceedsWithPolicyLevel) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  policy::PRegPolicyWriter writer(policy::GetRegistryKey());
  writer.AppendBoolean(policy::key::kSearchSuggestEnabled,
                       kPolicyBool,
                       policy::POLICY_LEVEL_RECOMMENDED);
  writer.AppendInteger(policy::key::kPolicyRefreshRate, kPolicyInt);
  writer.WriteToFile(stub_gpo1_path_);

  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_TRUE(policy.searchsuggestenabled().has_policy_options());
    EXPECT_EQ(em::PolicyOptions_PolicyMode_RECOMMENDED,
              policy.searchsuggestenabled().policy_options().mode());

    EXPECT_TRUE(policy.policyrefreshrate().has_policy_options());
    EXPECT_EQ(em::PolicyOptions_PolicyMode_MANDATORY,
              policy.policyrefreshrate().policy_options().mode());
  };
  EXPECT_EQ(ERROR_NONE,
            Join(kOneGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Verifies that a POLICY_LEVEL_MANDATORY policy is not overwritten by a
// POLICY_LEVEL_RECOMMENDED policy.
TEST_F(AuthPolicyTest, UserPolicyFetchMandatoryTakesPreference) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  policy::PRegPolicyWriter writer1(policy::GetRegistryKey());
  writer1.AppendBoolean(policy::key::kSearchSuggestEnabled,
                        kPolicyBool,
                        policy::POLICY_LEVEL_MANDATORY);
  writer1.WriteToFile(stub_gpo1_path_);

  // Normally, the latter GPO file overrides the former
  // (DevicePolicyFetchGposOverwrite), but POLICY_LEVEL_RECOMMENDED does not
  // beat POLICY_LEVEL_MANDATORY.
  policy::PRegPolicyWriter writer2(policy::GetRegistryKey());
  writer2.AppendBoolean(policy::key::kSearchSuggestEnabled,
                        kOtherPolicyBool,
                        policy::POLICY_LEVEL_RECOMMENDED);
  writer2.WriteToFile(stub_gpo2_path_);

  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_TRUE(policy.searchsuggestenabled().has_value());
    EXPECT_EQ(kPolicyBool, policy.searchsuggestenabled().value());
    EXPECT_TRUE(policy.searchsuggestenabled().has_policy_options());
    EXPECT_EQ(em::PolicyOptions_PolicyMode_MANDATORY,
              policy.searchsuggestenabled().policy_options().mode());
  };
  EXPECT_EQ(ERROR_NONE,
            Join(kTwoGposMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Verify that GPO containing policies with the wrong data type are not set.
// An exception is bool and int. Internally, bools are handled like ints with
// value {0,1}, so that setting kPolicyRefreshRate to true is actually
// interpreted as int 1.
TEST_F(AuthPolicyTest, UserPolicyFetchIgnoreBadDataType) {
  // Set policies with wrong data type, e.g. kPinnedLauncherApps is a string
  // list, but it is set as a string. See UserPolicyFetchSucceedsWithData for
  // the logic of policy testing.
  policy::PRegPolicyWriter writer(policy::GetRegistryKey());
  writer.AppendBoolean(policy::key::kPolicyRefreshRate, kPolicyBool);
  writer.AppendInteger(policy::key::kHomepageLocation, kPolicyInt);
  writer.AppendString(policy::key::kPinnedLauncherApps, kHomepageUrl);
  const std::vector<std::string> apps = {"App1", "App2"};
  writer.AppendStringList(policy::key::kSearchSuggestEnabled, apps);
  writer.WriteToFile(stub_gpo1_path_);

  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_FALSE(policy.has_searchsuggestenabled());
    EXPECT_FALSE(policy.has_pinnedlauncherapps());
    EXPECT_FALSE(policy.has_homepagelocation());
    EXPECT_TRUE(policy.has_policyrefreshrate());  // Interpreted as int 1.
  };
  EXPECT_EQ(ERROR_NONE,
            Join(kOneGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// GPOs with version 0 should be ignored.
TEST_F(AuthPolicyTest, UserPolicyFetchIgnoreZeroVersion) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  policy::PRegPolicyWriter writer(policy::GetRegistryKey());
  writer.AppendBoolean(policy::key::kSearchSuggestEnabled, kPolicyBool);
  writer.WriteToFile(stub_gpo1_path_);

  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_FALSE(policy.has_searchsuggestenabled());
  };
  EXPECT_EQ(
      ERROR_NONE,
      Join(kZeroUserVersionMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);

  // Validate the validation. GPO is actually taken if user version is > 0.
  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_TRUE(policy.has_searchsuggestenabled());
  };
  EXPECT_TRUE(MakeConfigWriteable());
  EXPECT_EQ(ERROR_NONE,
            Join(kOneGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(4, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// GPOs with an ignore flag set should be ignored. Sounds reasonable, hmm?
TEST_F(AuthPolicyTest, UserPolicyFetchIgnoreFlagSet) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  policy::PRegPolicyWriter writer(policy::GetRegistryKey());
  writer.AppendBoolean(policy::key::kSearchSuggestEnabled, kPolicyBool);
  writer.WriteToFile(stub_gpo1_path_);

  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_FALSE(policy.has_searchsuggestenabled());
  };
  EXPECT_EQ(
      ERROR_NONE,
      Join(kDisableUserFlagMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);

  // Validate the validation. GPO is taken if the ignore flag is not set.
  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_TRUE(policy.has_searchsuggestenabled());
  };
  EXPECT_TRUE(MakeConfigWriteable());
  EXPECT_EQ(ERROR_NONE,
            Join(kOneGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(4, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// User policy fetch should ignore GPO files that are missing on the server.
// This test looks for stub_gpo1_path_ and won't find it because we don't create
// it.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceedsMissingFile) {
  validate_user_policy_ = &CheckUserPolicyEmpty;
  EXPECT_EQ(ERROR_NONE,
            Join(kOneGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// User policy fetch fails if a file fails to download (unless it's missing,
// see UserPolicyFetchSucceedsMissingFile).
TEST_F(AuthPolicyTest, UserPolicyFetchFailsDownloadError) {
  EXPECT_EQ(
      ERROR_NONE,
      Join(kGpoDownloadErrorMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_SMBCLIENT_FAILED);
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Successful device policy fetch with empty policy.
TEST_F(AuthPolicyTest, DevicePolicyFetchSucceeds) {
  validate_device_policy_ = &CheckDevicePolicyEmpty;
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateDevicePolicy(ERROR_NONE);
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Device policy fetch fails if the machine account doesn't exist.
TEST_F(AuthPolicyTest, DevicePolicyFetchFailsBadMachineName) {
  validate_device_policy_ = &CheckDevicePolicyEmpty;
  EXPECT_EQ(ERROR_NONE,
            Join(kNonExistingMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateDevicePolicy(ERROR_BAD_MACHINE_NAME);
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// Successful device policy fetch with a few kinit retries because the machine
// account hasn't propagated yet.
TEST_F(AuthPolicyTest, DevicePolicyFetchSucceedsPropagationRetry) {
  validate_device_policy_ = &CheckDevicePolicyEmpty;
  EXPECT_EQ(
      ERROR_NONE,
      Join(kPropagationRetryMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateDevicePolicy(ERROR_NONE);
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(kNumPropagationRetries,
            metrics_->GetLastMetricSample(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Successful device policy fetch with actual data.
TEST_F(AuthPolicyTest, DevicePolicyFetchSucceedsWithData) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  policy::PRegPolicyWriter writer(policy::GetRegistryKey());
  writer.AppendBoolean(policy::key::kDeviceGuestModeEnabled, kPolicyBool);
  writer.AppendInteger(policy::key::kDeviceLocalAccountAutoLoginDelay,
                       kPolicyInt);
  writer.AppendString(policy::key::kSystemTimezone, kTimezone);
  const std::vector<std::string> flags = {"flag1", "flag2"};
  writer.AppendStringList(policy::key::kDeviceStartUpFlags, flags);
  writer.WriteToFile(stub_gpo1_path_);

  validate_device_policy_ = [flags](
                                const em::ChromeDeviceSettingsProto& policy) {
    EXPECT_EQ(kPolicyBool, policy.guest_mode_enabled().guest_mode_enabled());
    EXPECT_EQ(kPolicyInt, policy.device_local_accounts().auto_login_delay());
    EXPECT_EQ(kTimezone, policy.system_timezone().timezone());
    const em::StartUpFlagsProto& flags_proto = policy.start_up_flags();
    EXPECT_EQ(flags_proto.flags_size(), static_cast<int>(flags.size()));
    for (int n = 0; n < flags_proto.flags_size(); ++n)
      EXPECT_EQ(flags_proto.flags(n), flags.at(n));
  };
  EXPECT_EQ(ERROR_NONE,
            Join(kOneGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateDevicePolicy(ERROR_NONE);
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Completely empty GPO list fails. GPO lists should always contain at least
// a "local policy" created by the Samba tool, independently of the server.
TEST_F(AuthPolicyTest, DevicePolicyFetchFailsEmptyGpoList) {
  EXPECT_EQ(ERROR_NONE,
            Join(kEmptyGpoMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateDevicePolicy(ERROR_PARSE_FAILED);
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

// A GPO later in the list overrides prior GPOs.
TEST_F(AuthPolicyTest, DevicePolicyFetchGposOverride) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  policy::PRegPolicyWriter writer1(policy::GetRegistryKey());
  writer1.AppendBoolean(policy::key::kDeviceGuestModeEnabled, kOtherPolicyBool);
  writer1.AppendInteger(policy::key::kDeviceLocalAccountAutoLoginDelay,
                        kPolicyInt);
  writer1.AppendString(policy::key::kSystemTimezone, kTimezone);
  const std::vector<std::string> flags1 = {"flag1", "flag2", "flag3"};
  writer1.AppendStringList(policy::key::kDeviceStartUpFlags, flags1);
  writer1.WriteToFile(stub_gpo1_path_);

  policy::PRegPolicyWriter writer2(policy::GetRegistryKey());
  writer2.AppendBoolean(policy::key::kDeviceGuestModeEnabled, kPolicyBool);
  writer2.AppendInteger(policy::key::kDeviceLocalAccountAutoLoginDelay,
                        kOtherPolicyInt);
  writer2.AppendString(policy::key::kSystemTimezone, kAltTimezone);
  const std::vector<std::string> flags2 = {"flag4", "flag5"};
  writer2.AppendStringList(policy::key::kDeviceStartUpFlags, flags2);
  writer2.WriteToFile(stub_gpo2_path_);

  validate_device_policy_ = [flags2](
                                const em::ChromeDeviceSettingsProto& policy) {
    EXPECT_EQ(kPolicyBool, policy.guest_mode_enabled().guest_mode_enabled());
    EXPECT_EQ(kOtherPolicyInt,
              policy.device_local_accounts().auto_login_delay());
    EXPECT_EQ(kAltTimezone, policy.system_timezone().timezone());
    const em::StartUpFlagsProto& flags_proto = policy.start_up_flags();
    EXPECT_EQ(flags_proto.flags_size(), static_cast<int>(flags2.size()));
    for (int n = 0; n < flags_proto.flags_size(); ++n)
      EXPECT_EQ(flags_proto.flags(n), flags2.at(n));
  };
  EXPECT_EQ(ERROR_NONE,
            Join(kTwoGposMachineName, kUserPrincipal, MakePasswordFd()));
  FetchAndValidateDevicePolicy(ERROR_NONE);
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
  EXPECT_EQ(1,
            metrics_->GetMetricReportCount(METRIC_SMBCLIENT_FAILED_TRY_COUNT));
  EXPECT_EQ(1, metrics_->GetMetricReportCount(METRIC_DOWNLOAD_GPO_COUNT));
}

// Make sure cleaning state works.
TEST_F(AuthPolicyTest, CleanStateDir) {
  const base::FilePath state_path =
      base::FilePath(paths_->Get(Path::STATE_DIR));
  EXPECT_EQ(ERROR_NONE, Join(kMachineName, kUserPrincipal, MakePasswordFd()));
  EXPECT_EQ(ERROR_NONE, Auth(kUserPrincipal, "", MakePasswordFd()));
  EXPECT_FALSE(base::IsDirectoryEmpty(state_path));
  EXPECT_TRUE(AuthPolicy::CleanState(paths_.get()));
  EXPECT_TRUE(base::IsDirectoryEmpty(state_path));
  EXPECT_EQ(2, metrics_->GetMetricReportCount(METRIC_KINIT_FAILED_TRY_COUNT));
}

}  // namespace authpolicy
