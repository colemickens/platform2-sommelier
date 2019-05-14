// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/kerberos_adaptor.h"

#include <memory>
#include <utility>

#include <base/files/scoped_temp_dir.h>
#include <base/memory/ref_counted.h>
#include <dbus/login_manager/dbus-constants.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "kerberos/account_manager.h"
#include "kerberos/proto_bindings/kerberos_service.pb.h"

using brillo::dbus_utils::DBusObject;
using dbus::MockBus;
using dbus::MockExportedObject;
using dbus::MockObjectProxy;
using dbus::ObjectPath;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;
using ByteArray = kerberos::KerberosAdaptor::ByteArray;

namespace kerberos {
namespace {

// Some arbitrary D-Bus message serial number. Required for mocking D-Bus calls.
const int kDBusSerial = 123;

// Stub user data.
constexpr char kUser[] = "user";
constexpr char kUserHash[] = "user-hash";
constexpr char kPrincipalName[] = "user@REALM.COM";

// Stub D-Bus object path for the mock daemon.
constexpr char kObjectPath[] = "/object/path";

// Real storage base dir.
constexpr char KDaemonStore[] = "/run/daemon-store/kerberosd";

// Stub completion callback for RegisterAsync().
void DoNothing(bool /* unused */) {}

// Serializes |message| as byte array.
ByteArray SerializeAsBlob(const google::protobuf::MessageLite& message) {
  ByteArray result;
  result.resize(message.ByteSize());
  CHECK(message.SerializeToArray(result.data(), result.size()));
  return result;
}

// Parses a response message from a byte array.
template <typename TResponse>
TResponse ParseResponse(const ByteArray& response_blob) {
  TResponse response;
  EXPECT_TRUE(
      response.ParseFromArray(response_blob.data(), response_blob.size()));
  return response;
}

// Stub RetrievePrimarySession Session Manager method.
dbus::Response* StubRetrievePrimarySession(dbus::MethodCall* method_call,
                                           int /* timeout_ms */,
                                           dbus::ScopedDBusError* /* error */) {
  // Respond with username = kUser and sanitized_username = kUserHash.
  method_call->SetSerial(kDBusSerial);
  auto response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kUser);
  writer.AppendString(kUserHash);

  // Note: The mock wraps this back into a std::unique_ptr.
  return response.release();
}

}  // namespace

class KerberosAdaptorTest : public ::testing::Test {
 public:
  KerberosAdaptorTest() = default;
  ~KerberosAdaptorTest() override = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    mock_bus_ = make_scoped_refptr(new MockBus(dbus::Bus::Options()));

    // Mock out D-Bus initialization.
    const ObjectPath object_path(kObjectPath);
    mock_exported_object_ = make_scoped_refptr(
        new MockExportedObject(mock_bus_.get(), object_path));
    EXPECT_CALL(*mock_bus_, GetExportedObject(object_path))
        .WillRepeatedly(Return(mock_exported_object_.get()));
    EXPECT_CALL(*mock_exported_object_, Unregister()).Times(AnyNumber());
    EXPECT_CALL(*mock_exported_object_, ExportMethod(_, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_exported_object_, SendSignal(_))
        .WillRepeatedly(
            Invoke(this, &KerberosAdaptorTest::OnKerberosFilesChanged));

    // Create temp directory for files written during tests.
    CHECK(storage_dir_.CreateUniqueTempDir());

    // Create KerberosAdaptor instance. Do this AFTER creating the proxy mocks
    // since they might be accessed during initialization.
    auto dbus_object =
        std::make_unique<DBusObject>(nullptr, mock_bus_, object_path);
    adaptor_ = std::make_unique<KerberosAdaptor>(std::move(dbus_object));
    adaptor_->set_storage_dir_for_testing(storage_dir_.GetPath());
    adaptor_->RegisterAsync(base::BindRepeating(&DoNothing));
  }

 protected:
  void OnKerberosFilesChanged(dbus::Signal* signal) {
    EXPECT_EQ(signal->GetInterface(), "org.chromium.Kerberos");
    EXPECT_EQ(signal->GetMember(), "KerberosFilesChanged");
    dbus::MessageReader reader(signal);
    std::string principal_name;
    EXPECT_TRUE(reader.PopString(&principal_name));
    EXPECT_EQ(kPrincipalName, principal_name);
  }

  // Adds a default account.
  ErrorType AddAccount() {
    AddAccountRequest request;
    request.set_principal_name(kPrincipalName);
    request.set_is_managed(false);
    ByteArray response_blob = adaptor_->AddAccount(SerializeAsBlob(request));
    auto response = ParseResponse<AddAccountResponse>(response_blob);
    return response.error();
  }

  // Removes the default account.
  ErrorType RemoveAccount() {
    RemoveAccountRequest request;
    request.set_principal_name(kPrincipalName);
    ByteArray response_blob = adaptor_->RemoveAccount(SerializeAsBlob(request));
    auto response = ParseResponse<RemoveAccountResponse>(response_blob);
    return response.error();
  }

  // KEEP ORDER between these. It's important for destruction.
  scoped_refptr<MockBus> mock_bus_;
  scoped_refptr<MockExportedObject> mock_exported_object_;
  std::unique_ptr<KerberosAdaptor> adaptor_;

  base::ScopedTempDir storage_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KerberosAdaptorTest);
};

// RetrievePrimarySession is called to figure out the proper storage dir if the
// dir is NOT overwritten by KerberosAdaptor::set_storage_dir_for_testing().
TEST_F(KerberosAdaptorTest, RetrievesPrimarySession) {
  // Stub out Session Manager's RetrievePrimarySession D-Bus method.
  auto mock_session_manager_proxy = make_scoped_refptr(new MockObjectProxy(
      mock_bus_.get(), login_manager::kSessionManagerServiceName,
      dbus::ObjectPath(login_manager::kSessionManagerServicePath)));
  EXPECT_CALL(*mock_bus_,
              GetObjectProxy(login_manager::kSessionManagerServiceName, _))
      .WillOnce(Return(mock_session_manager_proxy.get()));
  EXPECT_CALL(*mock_session_manager_proxy,
              MockCallMethodAndBlockWithErrorDetails(_, _, _))
      .WillOnce(Invoke(&StubRetrievePrimarySession));

  // Recreate an adaptor, but don't call set_storage_dir_for_testing().
  auto dbus_object =
      std::make_unique<DBusObject>(nullptr, mock_bus_, ObjectPath(kObjectPath));
  auto adaptor = std::make_unique<KerberosAdaptor>(std::move(dbus_object));
  adaptor->RegisterAsync(base::BindRepeating(&DoNothing));

  // Check if the right storage dir is set.
  EXPECT_EQ(base::FilePath(KDaemonStore).Append(kUserHash),
            adaptor->GetAccountManagerForTesting()->GetStorageDirForTesting());
}

// AddAccount and RemoveAccount succeed when a new account is added and removed.
TEST_F(KerberosAdaptorTest, AddRemoveAccountSucceess) {
  EXPECT_EQ(ERROR_NONE, AddAccount());
  EXPECT_EQ(ERROR_NONE, RemoveAccount());
}

// TODO(https://crbug.com/952247): Add more tests.

}  // namespace kerberos
