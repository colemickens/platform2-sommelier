// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "container_utils/device_jail/permission_broker_client.h"

#include <memory>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/memory/ptr_util.h>
#include <base/memory/ref_counted.h>
#include <brillo/message_loops/base_message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/file_descriptor.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <gtest/gtest.h>

namespace device_jail {

namespace {

using testing::AnyNumber;
using testing::Return;
using testing::_;

const char kTestDevicePath[] = "/test/device";

MATCHER(IsOpenPathMethodCall, "") {
  return arg->GetInterface() == permission_broker::kPermissionBrokerInterface
      && arg->GetMember() == permission_broker::kOpenPath;
}

class PermissionBrokerClientTest : public testing::Test {
 public:
  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock object proxy.
    using permission_broker::kPermissionBrokerServiceName;
    using permission_broker::kPermissionBrokerServicePath;

    mock_object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(),
        kPermissionBrokerServiceName,
        dbus::ObjectPath(kPermissionBrokerServicePath));
    EXPECT_CALL(*bus_,
                GetObjectProxy(kPermissionBrokerServiceName,
                               dbus::ObjectPath(kPermissionBrokerServicePath)))
        .WillRepeatedly(Return(mock_object_proxy_.get()));

    // Set up the permission broker client.
    broker_client_ = base::MakeUnique<PermissionBrokerClient>(
        mock_object_proxy_.get(), &message_loop_);
  }

 protected:
  void ExpectSuccess() {
    std::unique_ptr<dbus::Response> response =
        dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    // We need to return a valid file descriptor. It doesn't matter which one,
    // so let's just use stdout.
    dbus::FileDescriptor fd(1);
    fd.CheckValidity();
    writer.AppendFileDescriptor(fd);
    EXPECT_CALL(*mock_object_proxy_,
                MockCallMethodAndBlock(IsOpenPathMethodCall(), _))
        .WillOnce(Return(response.release()));
  }

  void ExpectMalformedResponse() {
    // Return a response which has no FD.
    std::unique_ptr<dbus::Response> response =
        dbus::Response::CreateEmpty();
    dbus::MessageWriter(response.get());
    EXPECT_CALL(*mock_object_proxy_,
                MockCallMethodAndBlock(IsOpenPathMethodCall(), _))
        .WillOnce(Return(response.release()));
  }

  void ExpectEmptyResponse() {
    EXPECT_CALL(*mock_object_proxy_,
                MockCallMethodAndBlock(IsOpenPathMethodCall(), _))
        .WillOnce(Return(nullptr));
  }

  int OpenTestDevice() {
    int ret;
    broker_client_->Open(kTestDevicePath,
                         base::Bind(&PermissionBrokerClientTest::OpenCallback,
                                    base::Unretained(this),
                                    &ret));
    message_loop_.RunUntilIdle();
    return ret;
  }

 private:
  void OpenCallback(int* out_fd, int fd) {
    *out_fd = fd;
  }

  std::unique_ptr<PermissionBrokerClient> broker_client_;

  base::MessageLoop message_loop_;

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_object_proxy_;
};

}  // namespace

TEST_F(PermissionBrokerClientTest, ValidFD) {
  ExpectSuccess();
  EXPECT_GE(OpenTestDevice(), 0);
}

TEST_F(PermissionBrokerClientTest, MalformedResponse) {
  ExpectMalformedResponse();
  EXPECT_LT(OpenTestDevice(), 0);
}

TEST_F(PermissionBrokerClientTest, EmptyResponse) {
  ExpectEmptyResponse();
  EXPECT_LT(OpenTestDevice(), 0);
}

}  // namespace device_jail

int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
