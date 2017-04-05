// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy.h"

#include <utility>
#include <vector>

#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/utf_string_conversions.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <components/policy/core/common/policy_types.h>
#include <components/policy/core/common/preg_parser.h>
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
#include "authpolicy/proto_bindings/active_directory_account_data.pb.h"
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

const char kRecommendedKey[] = "\\Recommended";
const char kActionTriggerDelVals[] = "**delvals";

// Constants for PReg file delimiters.
const base::char16 kDelimBracketOpen = L'[';
const base::char16 kDelimBracketClose = L']';
const base::char16 kDelimSemicolon = L';';
const base::char16 kDelimZero = L'\0';

// Registry data type constants. Values have to match REG_* constants on
// Windows. Does not check data types or whether a policy can be recommended or
// not. This is checked when GPO is converted to a policy proto.
const uint32_t kRegSz = 1;
const uint32_t kRegDwordLittleEndian = 4;

// Helper class to write valid registry.pol ("PREG") files with the specified
// policy values. Test cases can create these files and expose them to
// authpolicy through stub_smbclient.
class PRegPolicyWriter {
 public:
  PRegPolicyWriter()
      : mandatory_key_(policy::helper::GetRegistryKey()),
        recommended_key_(mandatory_key_ + kRecommendedKey) {
    DCHECK(!mandatory_key_.empty());
    buffer_.append(policy::preg_parser::kPRegFileHeader,
                   policy::preg_parser::kPRegFileHeader +
                       arraysize(policy::preg_parser::kPRegFileHeader));
  }

  // Appends a boolean policy value.
  void AppendBoolean(
      const char* policy_name,
      bool value,
      policy::PolicyLevel level = policy::POLICY_LEVEL_MANDATORY) {
    StartEntry(
        GetKey(level), policy_name, kRegDwordLittleEndian, sizeof(uint32_t));
    AppendUnsignedInt(value ? 1 : 0);
    EndEntry();
  }

  // Appends an integer policy value.
  void AppendInteger(
      const char* policy_name,
      uint32_t value,
      policy::PolicyLevel level = policy::POLICY_LEVEL_MANDATORY) {
    StartEntry(
        GetKey(level), policy_name, kRegDwordLittleEndian, sizeof(uint32_t));
    AppendUnsignedInt(value);
    EndEntry();
  }

  // Appends a string policy value.
  void AppendString(
      const char* policy_name,
      const std::string& value,
      policy::PolicyLevel level = policy::POLICY_LEVEL_MANDATORY) {
    // size * 2 + 2 because of char16 and the 0-terminator.
    StartEntry(GetKey(level),
               policy_name,
               kRegSz,
               static_cast<uint32_t>(value.size()) * 2 + 2);
    AppendString(value);
    AppendChar16(kDelimZero);
    EndEntry();
  }

  // Appends a string list policy value.
  void AppendStringList(
      const char* policy_name,
      const std::vector<std::string>& values,
      policy::PolicyLevel level = policy::POLICY_LEVEL_MANDATORY) {
    // Add entry to wipe previous values.
    std::string key = GetKey(level) + "\\" + policy_name;
    StartEntry(key, kActionTriggerDelVals, kRegSz, 2);
    AppendString("");
    AppendChar16(kDelimZero);
    EndEntry();

    // Add an entry for each value.
    for (int n = 0; n < static_cast<int>(values.size()); ++n) {
      StartEntry(key,
                 base::IntToString(n + 1),
                 kRegSz,
                 static_cast<uint32_t>(values[n].size()) * 2 + 2);
      AppendString(values[n]);
      AppendChar16(kDelimZero);
      EndEntry();
    }
  }

  // Writes the policy data to a file. Returns true on success.
  bool WriteToFile(const base::FilePath& path) {
    int size = static_cast<int>(buffer_.size());
    return base::WriteFile(path, buffer_.data(), size) == size;
  }

 private:
  // Starts a policy entry.
  void StartEntry(const std::string& key_name,
                  const std::string& value_name,
                  uint32_t data_type,
                  uint32_t data_size) {
    AppendChar16(kDelimBracketOpen);

    AppendString(key_name);
    AppendChar16(kDelimZero);
    AppendChar16(kDelimSemicolon);

    AppendString(value_name);
    AppendChar16(kDelimZero);
    AppendChar16(kDelimSemicolon);

    AppendUnsignedInt(data_type);
    AppendChar16(kDelimSemicolon);

    AppendUnsignedInt(data_size);
    AppendChar16(kDelimSemicolon);
  }

  // Ends a policy entry.
  void EndEntry() { AppendChar16(kDelimBracketClose); }

  // Appends a string to the internal buffer. Note that all strings are written
  // as char16s.
  void AppendString(const std::string& str) {
    for (char ch : str)
      AppendChar16(static_cast<base::char16>(ch));
  }

  // Appends an unsigned integer to the internal buffer.
  void AppendUnsignedInt(uint32_t value) {
    AppendChar16(value & 0xffff);
    AppendChar16(value >> 16);
  }

  // Appends a char16 to the internal buffer.
  void AppendChar16(base::char16 ch) {
    buffer_.append(1, ch & 0xff);
    buffer_.append(1, ch >> 8);
  }

  // Returns the registry key that belongs to the given |level|.
  const std::string& GetKey(policy::PolicyLevel level) {
    return level == policy::POLICY_LEVEL_RECOMMENDED ? recommended_key_
                                                     : mandatory_key_;
  }

  std::string mandatory_key_;
  std::string recommended_key_;
  std::string buffer_;
};

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

    // Create path service with all paths pointing into a temp directory.
    CHECK(base::CreateNewTempDirectory("" /* prefix (ignored) */, &base_path_));
    auto paths = base::MakeUnique<TestPathService>(base_path_);

    // Create the state directory since authpolicyd assumes its existence.
    base::FilePath state_path(paths->Get(Path::STATE_DIR));
    CHECK(base::CreateDirectory(state_path));

    // Set stub preg path. Since it is not trivial to pass the full path to the
    // stub binaries, we simply use the directory from the krb5.conf file.
    const base::FilePath gpo_dir =
        base::FilePath(paths->Get(Path::USER_KRB5_CONF)).DirName();
    DCHECK(gpo_dir ==
           base::FilePath(paths->Get(Path::DEVICE_KRB5_CONF)).DirName());
    stub_gpo1_path_ = gpo_dir.Append(kGpo1Filename);
    stub_gpo2_path_ = gpo_dir.Append(kGpo2Filename);

    // Create AuthPolicy instance.
    authpolicy_ =
        base::MakeUnique<AuthPolicy>(std::move(dbus_object),
                                     base::MakeUnique<AuthPolicyMetrics>(),
                                     std::move(paths));
    EXPECT_EQ(ERROR_NONE, authpolicy_->Initialize(false /* expect_config */));

    // Mock out D-Bus initialization.
    mock_exported_object_ =
        new MockExportedObject(mock_bus_.get(), object_path);
    EXPECT_CALL(*mock_bus_, GetExportedObject(object_path))
        .Times(1)
        .WillOnce(Return(mock_exported_object_.get()));
    EXPECT_CALL(*mock_exported_object_.get(), ExportMethod(_, _, _, _))
        .Times(7);

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

    authpolicy_->RegisterAsync(base::Bind(&DoNothing));
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

    // Answer authpolicy with "true" to signal that policy has been stored.
    EXPECT_FALSE(callback.is_null());
    auto response = Response::CreateEmpty();
    MessageWriter writer(response.get());
    writer.AppendBool(true);
    callback.Run(response.get());
  }

  void TearDown() override {
    EXPECT_CALL(*mock_exported_object_, Unregister()).Times(1);
    // Don't not leave no mess behind.
    base::DeleteFile(base_path_, true /* recursive */);
  }

 protected:
  // Joins a (stub) Active Directory domain. Returns the error code.
  ErrorType Join(const std::string& machine_name) {
    return CastError(authpolicy_->JoinADDomain(
        machine_name, kUserPrincipal, MakePasswordFd()));
  }

  // Authenticates to a (stub) Active Directory domain with the given
  // credentials and returns the error code. If |account_id_key| is not nullptr,
  // assigns the (prefixed) account id key.
  ErrorType Auth(const std::string& user_principal,
                 dbus::FileDescriptor password_fd,
                 std::string* account_id_key = nullptr) {
    int32_t error = ERROR_NONE;
    std::vector<uint8_t> account_data_blob;
    authpolicy_->AuthenticateUser(
        user_principal, password_fd, &error, &account_data_blob);
    if (error == ERROR_NONE) {
      EXPECT_FALSE(account_data_blob.empty());
      authpolicy::ActiveDirectoryAccountData account_data;
      EXPECT_TRUE(account_data.ParseFromArray(
          account_data_blob.data(),
          static_cast<int>(account_data_blob.size())));
      if (account_id_key)
        *account_id_key = kActiveDirectoryPrefix + account_data.account_id();
    } else {
      EXPECT_TRUE(account_data_blob.empty());
    }
    return CastError(error);
  }

  // Authenticates to a (stub) Active Directory domain with default credentials.
  // Returns the account id key.
  std::string DefaultAuth() {
    std::string account_id_key;
    EXPECT_EQ(ERROR_NONE,
              Auth(kUserPrincipal, MakePasswordFd(), &account_id_key));
    return account_id_key;
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

  std::unique_ptr<base::MessageLoop> message_loop_;

  scoped_refptr<MockBus> mock_bus_ = new MockBus(dbus::Bus::Options());
  scoped_refptr<MockExportedObject> mock_exported_object_;
  scoped_refptr<MockObjectProxy> mock_session_manager_proxy_;
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
  int32_t error = ERROR_NONE;
  std::vector<uint8_t> account_data_blob;
  authpolicy_->AuthenticateUser(
      kUserPrincipal, MakePasswordFd(), &error, &account_data_blob);
  EXPECT_TRUE(account_data_blob.empty());
  EXPECT_EQ(ERROR_NOT_JOINED, error);
}

// Successful domain join.
TEST_F(AuthPolicyTest, JoinSucceeds) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
}

// Successful user authentication.
TEST_F(AuthPolicyTest, AuthSucceeds) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  EXPECT_EQ(ERROR_NONE, Auth(kUserPrincipal, MakePasswordFd()));
}

// Authentication fails for badly formatted user principal name.
TEST_F(AuthPolicyTest, AuthFailsInvalidUpn) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  EXPECT_EQ(ERROR_PARSE_UPN_FAILED,
            Auth(kInvalidUserPrincipal, MakePasswordFd()));
}

// Authentication fails for non-existing user principal name.
TEST_F(AuthPolicyTest, AuthFailsBadUpn) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  EXPECT_EQ(ERROR_BAD_USER_NAME,
            Auth(kNonExistingUserPrincipal, MakePasswordFd()));
}

// Authentication fails for wrong password.
TEST_F(AuthPolicyTest, AuthFailsBadPassword) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            Auth(kUserPrincipal, MakeFileDescriptor(kWrongPassword)));
}

// Authentication fails for expired password.
TEST_F(AuthPolicyTest, AuthFailsExpiredPassword) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  EXPECT_EQ(ERROR_PASSWORD_EXPIRED,
            Auth(kUserPrincipal, MakeFileDescriptor(kExpiredPassword)));
}

// Authentication fails if there's a network issue.
TEST_F(AuthPolicyTest, AuthFailsNetworkProblem) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  EXPECT_EQ(ERROR_NETWORK_PROBLEM,
            Auth(kNetworkErrorUserPrincipal, MakePasswordFd()));
}

// Authentication retries without KDC if it fails the first time.
TEST_F(AuthPolicyTest, AuthSucceedsKdcRetry) {
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  EXPECT_EQ(ERROR_NONE, Auth(kKdcRetryUserPrincipal, MakePasswordFd()));
}

// Join fails if there's a network issue.
TEST_F(AuthPolicyTest, JoinFailsNetworkProblem) {
  EXPECT_EQ(ERROR_NETWORK_PROBLEM,
            authpolicy_->JoinADDomain(
                kMachineName, kNetworkErrorUserPrincipal, MakePasswordFd()));
}

// Join fails for badly formatted user principal name.
TEST_F(AuthPolicyTest, JoinFailsInvalidUpn) {
  EXPECT_EQ(ERROR_PARSE_UPN_FAILED,
            authpolicy_->JoinADDomain(
                kMachineName, kInvalidUserPrincipal, MakePasswordFd()));
}

// Join fails for non-existing user principal name, but the error message is the
// same as for wrong password.
TEST_F(AuthPolicyTest, JoinFailsBadUpn) {
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            authpolicy_->JoinADDomain(
                kMachineName, kNonExistingUserPrincipal, MakePasswordFd()));
}

// Join fails for wrong password.
TEST_F(AuthPolicyTest, JoinFailsBadPassword) {
  EXPECT_EQ(
      ERROR_BAD_PASSWORD,
      authpolicy_->JoinADDomain(
          kMachineName, kUserPrincipal, MakeFileDescriptor(kWrongPassword)));
}

// Join fails if user can't join a machine to the domain.
TEST_F(AuthPolicyTest, JoinFailsAccessDenied) {
  EXPECT_EQ(ERROR_JOIN_ACCESS_DENIED,
            authpolicy_->JoinADDomain(
                kMachineName, kAccessDeniedUserPrincipal, MakePasswordFd()));
}

// Join fails if the machine name is too long.
TEST_F(AuthPolicyTest, JoinFailsMachineNameTooLong) {
  EXPECT_EQ(ERROR_MACHINE_NAME_TOO_LONG,
            authpolicy_->JoinADDomain(
                kTooLongMachineName, kUserPrincipal, MakePasswordFd()));
}

// Join fails if the machine name contains invalid characters.
TEST_F(AuthPolicyTest, JoinFailsBadMachineName) {
  EXPECT_EQ(ERROR_BAD_MACHINE_NAME,
            authpolicy_->JoinADDomain(
                kBadMachineName, kUserPrincipal, MakePasswordFd()));
}

// Join fails if the user can't join additional machines.
TEST_F(AuthPolicyTest, JoinFailsInsufficientQuota) {
  EXPECT_EQ(
      ERROR_USER_HIT_JOIN_QUOTA,
      authpolicy_->JoinADDomain(
          kMachineName, kInsufficientQuotaUserPrincipal, MakePasswordFd()));
}

// Successful user policy fetch with empty policy.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceeds) {
  // Verify that the GPO is actually empty.
  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    em::CloudPolicySettings empty_policy;
    EXPECT_EQ(policy.ByteSize(), empty_policy.ByteSize());
  };
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// Successful user policy fetch with actual data.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceedsWithData) {
  // Write a preg file with all basic data types. The file is picked up by
  // stub_net and "downloaded" by stub_smbclient.
  PRegPolicyWriter writer;
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
  EXPECT_EQ(ERROR_NONE, Join(kOneGpoMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// Verify that PolicyLevel is encoded properly.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceedsWithPolicyLevel) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  PRegPolicyWriter writer;
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
  EXPECT_EQ(ERROR_NONE, Join(kOneGpoMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// Verifies that a POLICY_LEVEL_MANDATORY policy is not overwritten by a
// POLICY_LEVEL_RECOMMENDED policy.
TEST_F(AuthPolicyTest, UserPolicyFetchMandatoryTakesPreference) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  PRegPolicyWriter writer1;
  writer1.AppendBoolean(policy::key::kSearchSuggestEnabled,
                        kPolicyBool,
                        policy::POLICY_LEVEL_MANDATORY);
  writer1.WriteToFile(stub_gpo1_path_);

  // Normally, the latter GPO file overrides the former
  // (DevicePolicyFetchGposOverwrite), but POLICY_LEVEL_RECOMMENDED does not
  // beat POLICY_LEVEL_MANDATORY.
  PRegPolicyWriter writer2;
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
  EXPECT_EQ(ERROR_NONE, Join(kTwoGposMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// Verify that GPO containing policies with the wrong data type are not set.
// An exception is bool and int. Internally, bools are handled like ints with
// value {0,1}, so that setting kPolicyRefreshRate to true is actually
// interpreted as int 1.
TEST_F(AuthPolicyTest, UserPolicyFetchIgnoreBadDataType) {
  // Set policies with wrong data type, e.g. kPinnedLauncherApps is a string
  // list, but it is set as a string. See UserPolicyFetchSucceedsWithData for
  // the logic of policy testing.
  PRegPolicyWriter writer;
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
  EXPECT_EQ(ERROR_NONE, Join(kOneGpoMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// GPOs with version 0 should be ignored.
TEST_F(AuthPolicyTest, UserPolicyFetchIgnoreZeroVersion) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  PRegPolicyWriter writer;
  writer.AppendBoolean(policy::key::kSearchSuggestEnabled, kPolicyBool);
  writer.WriteToFile(stub_gpo1_path_);

  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_FALSE(policy.has_searchsuggestenabled());
  };
  EXPECT_EQ(ERROR_NONE, Join(kZeroUserVersionMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);

  // Validate the validation. GPO is actually taken if user version is > 0.
  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_TRUE(policy.has_searchsuggestenabled());
  };
  EXPECT_EQ(ERROR_NONE, Join(kOneGpoMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// GPOs with an ignore flag set should be ignored. Sounds reasonable, hmm?
TEST_F(AuthPolicyTest, UserPolicyFetchIgnoreFlagSet) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  PRegPolicyWriter writer;
  writer.AppendBoolean(policy::key::kSearchSuggestEnabled, kPolicyBool);
  writer.WriteToFile(stub_gpo1_path_);

  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_FALSE(policy.has_searchsuggestenabled());
  };
  EXPECT_EQ(ERROR_NONE, Join(kDisableUserFlagMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);

  // Validate the validation. GPO is taken if the ignore flag is not set.
  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    EXPECT_TRUE(policy.has_searchsuggestenabled());
  };
  EXPECT_EQ(ERROR_NONE, Join(kOneGpoMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// User policy fetch should ignore GPO files that are missing on the server.
// This test looks for stub_gpo1_path_ and won't find it because we don't create
// it.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceedsMissingFile) {
  // Verify that the GPO is actually empty.
  validate_user_policy_ = [](const em::CloudPolicySettings& policy) {
    em::CloudPolicySettings empty_policy;
    EXPECT_EQ(policy.ByteSize(), empty_policy.ByteSize());
  };
  EXPECT_EQ(ERROR_NONE, Join(kOneGpoMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_NONE);
}

// User policy fetch fails if a file fails to download (unless it's missing,
// see UserPolicyFetchSucceedsMissingFile).
TEST_F(AuthPolicyTest, UserPolicyFetchFailsDownloadError) {
  EXPECT_EQ(ERROR_NONE, Join(kGpoDownloadErrorMachineName));
  FetchAndValidateUserPolicy(DefaultAuth(), ERROR_SMBCLIENT_FAILED);
}

// Successful device policy fetch with empty policy.
TEST_F(AuthPolicyTest, DevicePolicyFetchSucceeds) {
  // Verify that the GPO is actually empty.
  validate_device_policy_ = [](const em::ChromeDeviceSettingsProto& policy) {
    em::ChromeDeviceSettingsProto empty_policy;
    EXPECT_EQ(policy.ByteSize(), empty_policy.ByteSize());
  };
  EXPECT_EQ(ERROR_NONE, Join(kMachineName));
  FetchAndValidateDevicePolicy(ERROR_NONE);
}

// Successful device policy fetch with actual data.
TEST_F(AuthPolicyTest, DevicePolicyFetchSucceedsWithData) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  PRegPolicyWriter writer;
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
  EXPECT_EQ(ERROR_NONE, Join(kOneGpoMachineName));
  FetchAndValidateDevicePolicy(ERROR_NONE);
}

// Completely empty GPO list fails. GPO lists should always contain at least
// a "local policy" created by the Samba tool, independently of the server.
TEST_F(AuthPolicyTest, DevicePolicyFetchFailsEmptyGpoList) {
  EXPECT_EQ(ERROR_NONE, Join(kEmptyGpoMachineName));
  FetchAndValidateDevicePolicy(ERROR_PARSE_FAILED);
}

// A GPO later in the list overrides prior GPOs.
TEST_F(AuthPolicyTest, DevicePolicyFetchGposOverride) {
  // See UserPolicyFetchSucceedsWithData for the logic of policy testing.
  PRegPolicyWriter writer1;
  writer1.AppendBoolean(policy::key::kDeviceGuestModeEnabled, kOtherPolicyBool);
  writer1.AppendInteger(policy::key::kDeviceLocalAccountAutoLoginDelay,
                        kPolicyInt);
  writer1.AppendString(policy::key::kSystemTimezone, kTimezone);
  const std::vector<std::string> flags1 = {"flag1", "flag2", "flag3"};
  writer1.AppendStringList(policy::key::kDeviceStartUpFlags, flags1);
  writer1.WriteToFile(stub_gpo1_path_);

  PRegPolicyWriter writer2;
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
  EXPECT_EQ(ERROR_NONE, Join(kTwoGposMachineName));
  FetchAndValidateDevicePolicy(ERROR_NONE);
}

}  // namespace authpolicy
