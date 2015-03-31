// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <dbus/mock_object_proxy.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "attestation/client/dbus_proxy.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace attestation {

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

TEST_F(DBusProxyTest, CreateGoogleAttestedKeySuccess) {
  auto fake_dbus_call = [](dbus::MethodCall* method_call) -> dbus::Response* {
    // Verify request protobuf.
    dbus::MessageReader reader(method_call);
    CreateGoogleAttestedKeyRequest request_proto;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
    EXPECT_EQ("label", request_proto.key_label());
    EXPECT_EQ(KEY_TYPE_ECC, request_proto.key_type());
    EXPECT_EQ(KEY_USAGE_SIGN, request_proto.key_usage());
    EXPECT_EQ(ENTERPRISE_MACHINE_CERTIFICATE,
              request_proto.certificate_profile());
    // Create reply protobuf.
    scoped_ptr<dbus::Response> response =
        dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    CreateGoogleAttestedKeyReply reply_proto;
    reply_proto.set_status(SUCCESS);
    reply_proto.set_certificate("certificate");
    reply_proto.set_server_error("server_error");
    writer.AppendProtoAsArrayOfBytes(reply_proto);
    return response.release();
  };
  EXPECT_CALL(*mock_object_proxy_,
              MockCallMethodAndBlockWithErrorDetails(_, _, _))
      .WillOnce(WithArgs<0>(Invoke(fake_dbus_call)));
  std::string certificate;
  std::string server_error;
  EXPECT_EQ(SUCCESS, proxy_.CreateGoogleAttestedKey(
      "label", KEY_TYPE_ECC, KEY_USAGE_SIGN, ENTERPRISE_MACHINE_CERTIFICATE,
      &server_error, &certificate));
  EXPECT_EQ("certificate", certificate);
  EXPECT_EQ("server_error", server_error);
}

}  // namespace attestation
