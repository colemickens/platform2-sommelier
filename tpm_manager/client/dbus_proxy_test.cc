//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
    reply.set_status(STATUS_SUCCESS);
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
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
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
    reply.set_status(STATUS_SUCCESS);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const TakeOwnershipReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
  };
  TakeOwnershipRequest request;
  proxy_.TakeOwnership(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, DefineNvram) {
  uint32_t nvram_index = 5;
  size_t nvram_length = 32;
  auto fake_dbus_call = [nvram_index, nvram_length](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    DefineNvramRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    EXPECT_TRUE(request.has_index());
    EXPECT_EQ(nvram_index, request.index());
    EXPECT_TRUE(request.has_length());
    EXPECT_EQ(nvram_length, request.length());
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    DefineNvramReply reply;
    reply.set_status(STATUS_SUCCESS);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const DefineNvramReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
  };
  DefineNvramRequest request;
  request.set_index(nvram_index);
  request.set_length(nvram_length);
  proxy_.DefineNvram(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, DestroyNvram) {
  uint32_t nvram_index = 5;
  auto fake_dbus_call = [nvram_index](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    DestroyNvramRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    EXPECT_TRUE(request.has_index());
    EXPECT_EQ(nvram_index, request.index());
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    DestroyNvramReply reply;
    reply.set_status(STATUS_SUCCESS);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const DestroyNvramReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
  };
  DestroyNvramRequest request;
  request.set_index(nvram_index);
  proxy_.DestroyNvram(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}
TEST_F(DBusProxyTest, WriteNvram) {
  uint32_t nvram_index = 5;
  std::string nvram_data("nvram_data");
  auto fake_dbus_call = [nvram_index, nvram_data](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    WriteNvramRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    EXPECT_TRUE(request.has_index());
    EXPECT_EQ(nvram_index, request.index());
    EXPECT_TRUE(request.has_data());
    EXPECT_EQ(nvram_data, request.data());
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    WriteNvramReply reply;
    reply.set_status(STATUS_SUCCESS);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const WriteNvramReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
  };
  WriteNvramRequest request;
  request.set_index(nvram_index);
  request.set_data(nvram_data);
  proxy_.WriteNvram(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, ReadNvram) {
  uint32_t nvram_index = 5;
  std::string nvram_data("nvram_data");
  auto fake_dbus_call = [nvram_index, nvram_data](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    ReadNvramRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    EXPECT_TRUE(request.has_index());
    EXPECT_EQ(nvram_index, request.index());
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    ReadNvramReply reply;
    reply.set_status(STATUS_SUCCESS);
    reply.set_data(nvram_data);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count, nvram_data](const ReadNvramReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_data());
    EXPECT_EQ(nvram_data, reply.data());
  };
  ReadNvramRequest request;
  request.set_index(nvram_index);
  proxy_.ReadNvram(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, IsNvramDefined) {
  uint32_t nvram_index = 5;
  bool nvram_defined = true;
  auto fake_dbus_call = [nvram_index, nvram_defined](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    IsNvramDefinedRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    EXPECT_TRUE(request.has_index());
    EXPECT_EQ(nvram_index, request.index());
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    IsNvramDefinedReply reply;
    reply.set_status(STATUS_SUCCESS);
    reply.set_is_defined(nvram_defined);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count, nvram_defined](
      const IsNvramDefinedReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_is_defined());
    EXPECT_EQ(nvram_defined, reply.is_defined());
  };
  IsNvramDefinedRequest request;
  request.set_index(nvram_index);
  proxy_.IsNvramDefined(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, IsNvramLocked) {
  uint32_t nvram_index = 5;
  bool nvram_locked = true;
  auto fake_dbus_call = [nvram_index, nvram_locked](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    IsNvramLockedRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    EXPECT_TRUE(request.has_index());
    EXPECT_EQ(nvram_index, request.index());
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    IsNvramLockedReply reply;
    reply.set_status(STATUS_SUCCESS);
    reply.set_is_locked(nvram_locked);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count, nvram_locked](
      const IsNvramLockedReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_is_locked());
    EXPECT_EQ(nvram_locked, reply.is_locked());
  };
  IsNvramLockedRequest request;
  request.set_index(nvram_index);
  proxy_.IsNvramLocked(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, GetNvramSize) {
  uint32_t nvram_index = 5;
  size_t nvram_size = 32;
  auto fake_dbus_call = [nvram_index, nvram_size](
      dbus::MethodCall* method_call,
      const dbus::MockObjectProxy::ResponseCallback& response_callback) {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    GetNvramSizeRequest request;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request));
    EXPECT_TRUE(request.has_index());
    EXPECT_EQ(nvram_index, request.index());
    // Create reply protobuf.
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    GetNvramSizeReply reply;
    reply.set_status(STATUS_SUCCESS);
    reply.set_size(nvram_size);
    writer.AppendProtoAsArrayOfBytes(reply);
    response_callback.Run(response.release());
  };
  EXPECT_CALL(*mock_object_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count, nvram_size](
      const GetNvramSizeReply& reply) {
    callback_count++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_size());
    EXPECT_EQ(nvram_size, reply.size());
  };
  GetNvramSizeRequest request;
  request.set_index(nvram_index);
  proxy_.GetNvramSize(request, base::Bind(callback));
  EXPECT_EQ(1, callback_count);
}

}  // namespace tpm_manager
