// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <chromeos/bind_lambda.h>
#include <chromeos/dbus/dbus_object_test_helpers.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tpm_manager/common/dbus_interface.h"
#include "tpm_manager/common/mock_tpm_manager_interface.h"
#include "tpm_manager/server/dbus_service.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace tpm_manager {

class DBusServiceTest : public testing::Test {
 public:
  ~DBusServiceTest() override = default;
  void SetUp() override {
    dbus::Bus::Options options;
    mock_bus_ = new NiceMock<dbus::MockBus>(options);
    dbus::ObjectPath path(kTpmManagerServicePath);
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
        kTpmManagerInterface, method_name));
    call->SetSerial(1);
    return call;
  }

 protected:
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  StrictMock<MockTpmManagerInterface> mock_service_;
  std::unique_ptr<DBusService> dbus_service_;
};

TEST_F(DBusServiceTest, CopyableCallback) {
  EXPECT_CALL(mock_service_, GetTpmStatus(_, _))
      .WillOnce(WithArgs<1>(
          Invoke([](const TpmManagerInterface::GetTpmStatusCallback& callback) {
            // Copy the callback, then call the original.
            GetTpmStatusReply reply;
            base::Closure copy = base::Bind(callback, reply);
            callback.Run(reply);
          })));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(kGetTpmStatus);
  GetTpmStatusRequest request;
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  GetTpmStatusReply reply;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply));
}

TEST_F(DBusServiceTest, GetTpmStatus) {
  GetTpmStatusRequest request;
  EXPECT_CALL(mock_service_, GetTpmStatus(_, _))
      .WillOnce(Invoke([](
          const GetTpmStatusRequest& request,
          const TpmManagerInterface::GetTpmStatusCallback& callback) {
        GetTpmStatusReply reply;
        reply.set_status(STATUS_NOT_AVAILABLE);
        reply.set_enabled(true);
        reply.set_owned(true);
        reply.mutable_local_data()->set_owned_by_this_install(true);
        reply.set_dictionary_attack_counter(3);
        reply.set_dictionary_attack_threshold(4);
        reply.set_dictionary_attack_lockout_in_effect(true);
        reply.set_dictionary_attack_lockout_seconds_remaining(5);
        callback.Run(reply);
      }));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(kGetTpmStatus);
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  GetTpmStatusReply reply;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply));
  EXPECT_EQ(STATUS_NOT_AVAILABLE, reply.status());
  EXPECT_TRUE(reply.enabled());
  EXPECT_TRUE(reply.owned());
  EXPECT_TRUE(reply.local_data().owned_by_this_install());
  EXPECT_EQ(3, reply.dictionary_attack_counter());
  EXPECT_EQ(4, reply.dictionary_attack_threshold());
  EXPECT_TRUE(reply.dictionary_attack_lockout_in_effect());
  EXPECT_EQ(5, reply.dictionary_attack_lockout_seconds_remaining());
}

TEST_F(DBusServiceTest, TakeOwnership) {
  TakeOwnershipRequest request;
  EXPECT_CALL(mock_service_, TakeOwnership(_, _))
      .WillOnce(Invoke([](
          const TakeOwnershipRequest& request,
          const TpmManagerInterface::TakeOwnershipCallback& callback) {
        TakeOwnershipReply reply;
        reply.set_status(STATUS_NOT_AVAILABLE);
        callback.Run(reply);
      }));
  std::unique_ptr<dbus::MethodCall> call = CreateMethodCall(kTakeOwnership);
  dbus::MessageWriter writer(call.get());
  writer.AppendProtoAsArrayOfBytes(request);
  auto response = CallMethod(call.get());
  dbus::MessageReader reader(response.get());
  TakeOwnershipReply reply;
  EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&reply));
  EXPECT_EQ(STATUS_NOT_AVAILABLE, reply.status());
}

}  // namespace tpm_manager
