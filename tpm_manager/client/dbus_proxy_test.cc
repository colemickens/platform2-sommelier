// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <chromeos/bind_lambda.h>
#include <dbus/mock_object_proxy.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tpm_manager/client/dbus_proxy.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace tpm_manager {

class DBusProxyTest : public testing::Test {
 public:
  ~DBusProxyTest() override = default;
  void SetUp() override {
    mock_object_proxy_ = new StrictMock<dbus::MockObjectProxy>(
        nullptr, "", dbus::ObjectPath(""));
    proxy_.set_object_proxy(mock_object_proxy_.get());
  }
 protected:
  scoped_refptr<StrictMock<dbus::MockObjectProxy>> mock_object_proxy_;
  DBusProxy proxy_;
};

TEST_F(DBusProxyTest, GetTpmStatus) {
  auto fake_dbus_call = [](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    GetTpmStatusRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    GetTpmStatusReply reply;
    reply.set_status(STATUS_NOT_AVAILABLE);
    reply.set_enabled(true);
    reply.set_owned(true);
    reply.mutable_local_data()->set_owned_by_this_install(true);
    reply.set_dictionary_attack_counter(3);
    reply.set_dictionary_attack_threshold(4);
    reply.set_dictionary_attack_lockout_in_effect(true);
    reply.set_dictionary_attack_lockout_seconds_remaining(5);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const GetTpmStatusReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_NOT_AVAILABLE, reply.status());
    EXPECT_TRUE(reply.enabled());
    EXPECT_TRUE(reply.owned());
    EXPECT_TRUE(reply.local_data().owned_by_this_install());
    EXPECT_EQ(3, reply.dictionary_attack_counter());
    EXPECT_EQ(4, reply.dictionary_attack_threshold());
    EXPECT_TRUE(reply.dictionary_attack_lockout_in_effect());
    EXPECT_EQ(5, reply.dictionary_attack_lockout_seconds_remaining());
  };
  GetTpmStatusRequest request;
  proxy_.GetTpmStatus(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, TakeOwnership) {
  auto fake_dbus_call = [](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    TakeOwnershipRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    TakeOwnershipReply reply;
    reply.set_status(STATUS_NOT_AVAILABLE);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const TakeOwnershipReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_NOT_AVAILABLE, reply.status());
  };
  TakeOwnershipRequest request;
  proxy_.TakeOwnership(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

}  // namespace tpm_manager
