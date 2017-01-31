// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy.h"

#include <utility>

#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
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

const char kMachineName[] = "TESTCOMP";
const char kUserPrincipal[] = "user@realm.com";
const char kPassword[] = "p4zzw!5d";

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

// Fake completion callback for RegisterAsync().
void DoNothing(bool unused) {}

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
    Insert(Path::NET, stub_path.Append("stub_net").value());
    Insert(Path::SMBCLIENT, stub_path.Append("stub_smbclient").value());

    // TODO(ljusten): Remove when authpolicy is disabled on x86. crbug/693471.
    Insert(Path::DEBUG_FLAGS, base_path.Append("authpolicyd_flags").value());

    // Fill in the rest of the paths and build dependend paths.
    Initialize();
  }
};

// Helper to check the ErrorType value returned by authpolicy D-Bus calls.
void CheckError(ErrorType expected_error, std::unique_ptr<Response> response) {
  EXPECT_TRUE(response.get());
  dbus::MessageReader reader(response.get());
  int32_t int_error;
  EXPECT_TRUE(reader.PopInt32(&int_error));
  EXPECT_GE(int_error, 0);
  EXPECT_LT(int_error, ERROR_COUNT);
  ErrorType actual_error = static_cast<ErrorType>(int_error);
  EXPECT_EQ(expected_error, actual_error);
}

}  // namespace

// End-to-end test for the authpolicyd D-Bus interface.
//
// Since the Active Directory protocols are a black box to us, a stub local
// server cannot be used. Instead, the Samba/Kerberos binaries are stubbed out.
//
// During policy fetch, authpolicy sends D-Bus messages to Session Manager. This
// communication is mocked out.
class AuthPolicyTest : public testing::Test {
 public:
  void SetUp() override {
    const ObjectPath object_path(std::string("/object/path"));
    auto dbus_object =
        base::MakeUnique<DBusObject>(nullptr, mock_bus_, object_path);
    authpolicy_.reset(new AuthPolicy(std::move(dbus_object)));

    // Create path service with all paths pointing into a temp directory.
    CHECK(base::CreateNewTempDirectory("" /* prefix (ignored) */, &base_path_));
    auto paths = base::MakeUnique<TestPathService>(base_path_);

    // Create the state directory since authpolicyd assumes its existence.
    base::FilePath state_path(paths->Get(Path::STATE_DIR));
    CHECK(base::CreateDirectory(state_path));

    // Workaround for seccomp filters containing syscalls that don't exist on
    // x86. Turning seccomp logging on will ignore this error.
    // TODO(ljusten): Remove when authpolicy is disabled on x86. crbug/693471.
    const char kDebugFlags[] = "log_seccomp";
    LOG(INFO) << paths->Get(Path::DEBUG_FLAGS);
    CHECK_NE(-1,
             base::WriteFile(base::FilePath(paths->Get(Path::DEBUG_FLAGS)),
                             kDebugFlags,
                             strlen(kDebugFlags)));

    // Initialize authpolicy and mock out D-Bus initialization.
    EXPECT_EQ(
        ERROR_NONE,
        authpolicy_->Initialize(std::move(paths), false /* expect_config */));
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
    password_fd_ = MakeFileDescriptor(kPassword);
  }

  void TearDown() override {
    EXPECT_CALL(*mock_exported_object_, Unregister()).Times(1);
    // Don't not leave no mess behind.
    base::DeleteFile(base_path_, true /* recursive */);
  }

  scoped_refptr<MockBus> mock_bus_ = new MockBus(dbus::Bus::Options());
  scoped_refptr<MockExportedObject> mock_exported_object_;
  scoped_refptr<MockObjectProxy> mock_session_manager_proxy_;
  ObjectProxy::ResponseCallback policy_store_callback_;
  std::unique_ptr<AuthPolicy> authpolicy_;
  base::FilePath base_path_;
  dbus::FileDescriptor password_fd_;
};

// Can't fetch policy if the user is not logged in.
TEST_F(AuthPolicyTest, UserPolicyFailsNotLoggedIn) {
  std::string account_id;
  dbus::MethodCall method_call(kAuthPolicyInterface,
                               kAuthPolicyRefreshUserPolicy);
  method_call.SetSerial(123);
  AuthPolicy::PolicyResponseCallback callback =
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
          &method_call, base::Bind(&CheckError, ERROR_NOT_LOGGED_IN));
  authpolicy_->RefreshUserPolicy(std::move(callback), "account_id");
}

// Authentication fails if the machine is not joined.
TEST_F(AuthPolicyTest, AuthFailsNotJoined) {
  int32_t error = ERROR_NONE;
  std::string account_id;
  authpolicy_->AuthenticateUser(
      kUserPrincipal, password_fd_, &error, &account_id);
  EXPECT_TRUE(account_id.empty());
  EXPECT_EQ(ERROR_NOT_JOINED, error);
}

// Successful domain join.
TEST_F(AuthPolicyTest, JoinSucceeds) {
  EXPECT_EQ(
      ERROR_NONE,
      authpolicy_->JoinADDomain(kMachineName, kUserPrincipal, password_fd_));
}

// Successful domain join and user auth.
TEST_F(AuthPolicyTest, JoinAndDevicePolicyFetchSucceed) {
  EXPECT_EQ(
      ERROR_NONE,
      authpolicy_->JoinADDomain(kMachineName, kUserPrincipal, password_fd_));
  std::string account_id;
  dbus::MethodCall method_call(kAuthPolicyInterface,
                               kAuthPolicyRefreshDevicePolicy);
  method_call.SetSerial(123);
  AuthPolicy::PolicyResponseCallback callback =
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<int32_t>>(
          &method_call, base::Bind(&CheckError, ERROR_NONE));
  authpolicy_->RefreshDevicePolicy(std::move(callback));

  // Authpolicy should now call our mock Session Manager to store policy,
  // answer "true".
  EXPECT_FALSE(policy_store_callback_.is_null());
  auto response = Response::CreateEmpty();
  MessageWriter writer(response.get());
  writer.AppendBool(true);
  policy_store_callback_.Run(response.get());
}

}  // namespace authpolicy
