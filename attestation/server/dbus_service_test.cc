// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <chromeos/bind_lambda.h>
#include <chromeos/dbus/dbus_object_test_helpers.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "attestation/common/dbus_interface.h"
#include "attestation/common/mock_attestation_interface.h"
#include "attestation/server/dbus_service.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace attestation {

class DBusServiceTest : public testing::Test {
 public:
  ~DBusServiceTest() override = default;
  void SetUp() override {
    dbus::Bus::Options options;
    mock_bus_ = new NiceMock<dbus::MockBus>(options);
    dbus::ObjectPath path(kAttestationServicePath);
    mock_exported_object_ = new NiceMock<dbus::MockExportedObject>(
        mock_bus_.get(), path);
    ON_CALL(*mock_bus_, GetExportedObject(path))
        .WillByDefault(Return(mock_exported_object_.get()));
    dbus_service_.reset(new DBusService(mock_bus_, &mock_service_));
    dbus_service_->Register(chromeos::dbus_utils::AsyncEventSequencer::
                                GetDefaultCompletionAction());
  }

  std::unique_ptr<dbus::Response> CallMethod(dbus::MethodCall* method_call) {
    return chromeos::dbus_utils::testing::CallMethod(
        dbus_service_->dbus_object_, method_call);
  }

  std::unique_ptr<dbus::MethodCall> CreateMethodCall(
      const std::string& method_name) {
    std::unique_ptr<dbus::MethodCall> call(new dbus::MethodCall(
        kAttestationInterface, method_name));
    call->SetSerial(1);
    return call;
  }

 protected:
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  StrictMock<MockAttestationInterface> mock_service_;
  std::unique_ptr<DBusService> dbus_service_;
};

TEST_F(DBusServiceTest, CreateGoogleAttestedKey) {
  CreateGoogleAttestedKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_ECC);
  request.set_key_usage(KEY_USAGE_SIGN);
  request.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_username("username");
  request.set_origin("origin");
  EXPECT_CALL(mock_service_, CreateGoogleAttestedKey(_, _))
      .WillOnce(Invoke([](
            const CreateGoogleAttestedKeyRequest& request,
            const AttestationInterface::
                CreateGoogleAttestedKeyCallback& callback) {
        EXPECT_EQ("label", request.key_label());
        EXPECT_EQ(KEY_TYPE_ECC, request.key_type());
        EXPECT_EQ(KEY_USAGE_SIGN, request.key_usage());
        EXPECT_EQ(ENTERPRISE_MACHINE_CERTIFICATE,
                  request.certificate_profile());
        EXPECT_EQ("username", request.username());
        EXPECT_EQ("origin", request.origin());
        CreateGoogleAttestedKeyReply reply;
        reply.set_status(STATUS_SUCCESS);
        reply.set_certificate_chain("certificate");
        reply.set_server_error("server_error");
        callback.Run(reply);
      }));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(
      kCreateGoogleAttestedKey);
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  CreateGoogleAttestedKeyReply reply;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply));
  EXPECT_EQ(STATUS_SUCCESS, reply.status());
  EXPECT_EQ("certificate", reply.certificate_chain());
  EXPECT_EQ("server_error", reply.server_error());
}

TEST_F(DBusServiceTest, CopyableCallback) {
  EXPECT_CALL(mock_service_, CreateGoogleAttestedKey(_, _))
      .WillOnce(WithArgs<1>(Invoke([](const AttestationInterface::
          CreateGoogleAttestedKeyCallback& callback) {
        // Copy the callback, then call the original.
        CreateGoogleAttestedKeyReply reply;
        base::Closure copy = base::Bind(callback, reply);
        callback.Run(reply);
      })));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(
      kCreateGoogleAttestedKey);
  CreateGoogleAttestedKeyRequest request;
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  CreateGoogleAttestedKeyReply reply;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply));
}

}  // namespace attestation
