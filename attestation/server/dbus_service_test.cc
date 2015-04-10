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

TEST_F(DBusServiceTest, CreateGoogleAttestedKeySuccess) {
  EXPECT_CALL(mock_service_, CreateGoogleAttestedKey(
      "label", KEY_TYPE_ECC, KEY_USAGE_SIGN, ENTERPRISE_MACHINE_CERTIFICATE, _))
      .WillOnce(WithArgs<4>(Invoke([](const base::Callback<
          AttestationInterface::CreateGoogleAttestedKeyCallback>& callback) {
        callback.Run(SUCCESS, "certificate", "server_error");
      })));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(
      kCreateGoogleAttestedKey);
  CreateGoogleAttestedKeyRequest request_proto;
  request_proto.set_key_label("label");
  request_proto.set_key_type(KEY_TYPE_ECC);
  request_proto.set_key_usage(KEY_USAGE_SIGN);
  request_proto.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request_proto);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  CreateGoogleAttestedKeyReply reply_proto;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply_proto));
  EXPECT_EQ(SUCCESS, reply_proto.status());
  EXPECT_EQ("certificate", reply_proto.certificate_chain());
  EXPECT_EQ("", reply_proto.server_error());
}

TEST_F(DBusServiceTest, CreateGoogleAttestedKeyServerError) {
  EXPECT_CALL(mock_service_, CreateGoogleAttestedKey(_, _, _, _, _))
      .WillOnce(WithArgs<4>(Invoke([](const base::Callback<
          AttestationInterface::CreateGoogleAttestedKeyCallback>& callback) {
        callback.Run(REQUEST_DENIED_BY_CA, "certificate", "server_error");
      })));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(
      kCreateGoogleAttestedKey);
  CreateGoogleAttestedKeyRequest request_proto;
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request_proto);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  CreateGoogleAttestedKeyReply reply_proto;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply_proto));
  EXPECT_EQ(REQUEST_DENIED_BY_CA, reply_proto.status());
  EXPECT_EQ("", reply_proto.certificate_chain());
  EXPECT_EQ("server_error", reply_proto.server_error());
}

TEST_F(DBusServiceTest, CopyableCallback) {
  EXPECT_CALL(mock_service_, CreateGoogleAttestedKey(_, _, _, _, _))
      .WillOnce(WithArgs<4>(Invoke([](const base::Callback<
          AttestationInterface::CreateGoogleAttestedKeyCallback>& callback) {
        // Copy the callback, then call the original.
        base::Closure copy = base::Bind(callback, SUCCESS, "", "");
        callback.Run(SUCCESS, "", "");
      })));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(
      kCreateGoogleAttestedKey);
  CreateGoogleAttestedKeyRequest request_proto;
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request_proto);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  CreateGoogleAttestedKeyReply reply_proto;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply_proto));
}

}  // namespace attestation
