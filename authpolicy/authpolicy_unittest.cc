// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy.h"

#include <utility>

#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
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
#include "authpolicy/proto_bindings/active_directory_account_data.pb.h"
#include "authpolicy/samba_interface.h"
#include "authpolicy/stub_common.h"

using brillo::dbus_utils::DBusObject;
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

namespace authpolicy {
namespace {

// Some arbitrary D-Bus message serial number. Required for mocking D-Bus calls.
const int kDBusSerial = 123;

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

// Fake completion callback for RegisterAsync().
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
        .WillRepeatedly(SaveArg<2>(&policy_store_callback_));

    authpolicy_->RegisterAsync(base::Bind(&DoNothing));
  }

  void TearDown() override {
    EXPECT_CALL(*mock_exported_object_, Unregister()).Times(1);
    // Don't not leave no mess behind.
    base::DeleteFile(base_path_, true /* recursive */);
  }

 protected:
  // Joins a (stub) Active Directory domain. Returns the error code.
  ErrorType Join() {
    return CastError(authpolicy_->JoinADDomain(
        kMachineName, kUserPrincipal, MakePasswordFd()));
  }

  // A authenticates to a (stub) Active Directory domain with the given
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

  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<MockBus> mock_bus_ = new MockBus(dbus::Bus::Options());
  scoped_refptr<MockExportedObject> mock_exported_object_;
  scoped_refptr<MockObjectProxy> mock_session_manager_proxy_;
  ObjectProxy::ResponseCallback policy_store_callback_;
  std::unique_ptr<AuthPolicy> authpolicy_;
  base::FilePath base_path_;
};

// Can't fetch user policy if the user is not logged in.
TEST_F(AuthPolicyTest, UserPolicyFailsNotLoggedIn) {
  dbus::MethodCall method_call(kAuthPolicyInterface,
                               kAuthPolicyRefreshUserPolicy);
  method_call.SetSerial(kDBusSerial);
  bool callback_was_called = false;
  AuthPolicy::PolicyResponseCallback callback =
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
          &method_call,
          base::Bind(&CheckError, ERROR_NOT_LOGGED_IN, &callback_was_called));
  authpolicy_->RefreshUserPolicy(std::move(callback), "account_id");

  // RefreshUserPolicy() should early out and call the callback immediately,
  // the asynchronous method call to SessionManager shouldn't be queued.
  EXPECT_TRUE(callback_was_called);
  EXPECT_TRUE(policy_store_callback_.is_null());
}

// Can't fetch device policy if the device is not joined.
TEST_F(AuthPolicyTest, DevicePolicyFailsNotJoined) {
  dbus::MethodCall method_call(kAuthPolicyInterface,
                               kAuthPolicyRefreshDevicePolicy);
  method_call.SetSerial(kDBusSerial);
  bool callback_was_called = false;
  AuthPolicy::PolicyResponseCallback callback =
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
          &method_call,
          base::Bind(&CheckError, ERROR_NOT_JOINED, &callback_was_called));
  authpolicy_->RefreshDevicePolicy(std::move(callback));

  // RefreshDevicePolicy() should early out and call the callback immediately,
  // the asynchronous method call to SessionManager shouldn't be queued.
  EXPECT_TRUE(callback_was_called);
  EXPECT_TRUE(policy_store_callback_.is_null());
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
  EXPECT_EQ(ERROR_NONE, Join());
}

// Successful user authentication.
TEST_F(AuthPolicyTest, AuthSucceeds) {
  EXPECT_EQ(ERROR_NONE, Join());
  EXPECT_EQ(ERROR_NONE, Auth(kUserPrincipal, MakePasswordFd()));
}

// Authentication fails for badly formatted user principal name.
TEST_F(AuthPolicyTest, AuthFailsInvalidUpn) {
  EXPECT_EQ(ERROR_NONE, Join());
  EXPECT_EQ(ERROR_PARSE_UPN_FAILED,
            Auth(kInvalidUserPrincipal, MakePasswordFd()));
}

// Authentication fails for non-existing user principal name.
TEST_F(AuthPolicyTest, AuthFailsBadUpn) {
  EXPECT_EQ(ERROR_NONE, Join());
  EXPECT_EQ(ERROR_BAD_USER_NAME,
            Auth(kNonExistingUserPrincipal, MakePasswordFd()));
}

// Authentication fails for wrong password.
TEST_F(AuthPolicyTest, AuthFailsBadPassword) {
  EXPECT_EQ(ERROR_NONE, Join());
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            Auth(kUserPrincipal, MakeFileDescriptor(kWrongPassword)));
}

// Authentication fails for expired password.
TEST_F(AuthPolicyTest, AuthFailsExpiredPassword) {
  EXPECT_EQ(ERROR_NONE, Join());
  EXPECT_EQ(ERROR_PASSWORD_EXPIRED,
            Auth(kUserPrincipal, MakeFileDescriptor(kExpiredPassword)));
}

// Authentication fails if there's a network issue.
TEST_F(AuthPolicyTest, AuthFailsNetworkProblem) {
  EXPECT_EQ(ERROR_NONE, Join());
  EXPECT_EQ(ERROR_NETWORK_PROBLEM,
            Auth(kNetworkErrorUserPrincipal, MakePasswordFd()));
}

// Authentication fails if there's a network issue.
TEST_F(AuthPolicyTest, AuthSucceedsKdcRetry) {
  EXPECT_EQ(ERROR_NONE, Join());
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

// Successful device policy fetch.
TEST_F(AuthPolicyTest, UserPolicyFetchSucceeds) {
  std::string account_id_key;
  EXPECT_EQ(ERROR_NONE, Join());
  EXPECT_EQ(ERROR_NONE,
            Auth(kUserPrincipal, MakePasswordFd(), &account_id_key));

  dbus::MethodCall method_call(kAuthPolicyInterface,
                               kAuthPolicyRefreshUserPolicy);
  method_call.SetSerial(kDBusSerial);
  bool callback_was_called = false;
  AuthPolicy::PolicyResponseCallback callback =
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
          &method_call,
          base::Bind(&CheckError, ERROR_NONE, &callback_was_called));
  authpolicy_->RefreshUserPolicy(std::move(callback), account_id_key);

  // Authpolicy should have queued a call to Session Manager to store policy,
  // Answering "true" should call our |callback|.
  EXPECT_FALSE(callback_was_called);
  EXPECT_FALSE(policy_store_callback_.is_null());
  auto response = Response::CreateEmpty();
  MessageWriter writer(response.get());
  writer.AppendBool(true);
  policy_store_callback_.Run(response.get());
  EXPECT_TRUE(callback_was_called);
}

// Successful device policy fetch.
TEST_F(AuthPolicyTest, DevicePolicyFetchSucceeds) {
  EXPECT_EQ(ERROR_NONE, Join());
  std::string account_id;
  dbus::MethodCall method_call(kAuthPolicyInterface,
                               kAuthPolicyRefreshDevicePolicy);
  method_call.SetSerial(kDBusSerial);
  bool callback_was_called = false;
  AuthPolicy::PolicyResponseCallback callback =
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
          &method_call,
          base::Bind(&CheckError, ERROR_NONE, &callback_was_called));
  authpolicy_->RefreshDevicePolicy(std::move(callback));

  // Authpolicy should have queued a call to Session Manager to store policy,
  // Answering "true" should call our |callback|.
  EXPECT_FALSE(callback_was_called);
  EXPECT_FALSE(policy_store_callback_.is_null());
  auto response = Response::CreateEmpty();
  MessageWriter writer(response.get());
  writer.AppendBool(true);
  policy_store_callback_.Run(response.get());
  EXPECT_TRUE(callback_was_called);
}

}  // namespace authpolicy
