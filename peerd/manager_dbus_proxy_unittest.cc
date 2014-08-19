// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager_dbus_proxy.h"

#include <string>

#include <base/bind.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/dbus_constants.h"
#include "peerd/mock_manager.h"

using peerd::dbus_constants::kErrorTooManyParameters;
using peerd::dbus_constants::kManagerExposeIpService;
using peerd::dbus_constants::kManagerInterface;
using peerd::dbus_constants::kManagerPing;
using peerd::dbus_constants::kManagerRemoveExposedService;
using peerd::dbus_constants::kManagerServicePath;
using peerd::dbus_constants::kManagerSetFriendlyName;
using peerd::dbus_constants::kManagerSetNote;
using peerd::dbus_constants::kManagerStartMonitoring;
using peerd::dbus_constants::kManagerStopMonitoring;
using peerd::dbus_constants::kPingResponse;
using std::string;
using std::unique_ptr;
using testing::AnyNumber;
using testing::Return;
using testing::_;

namespace {

const char kSingleStringArgument[] = "Expected single string argument.";

unique_ptr<dbus::MethodCall> MakeMethodCall(const char* method_name) {
  auto ret = unique_ptr<dbus::MethodCall>(
      new dbus::MethodCall(kManagerInterface, method_name));
  ret->SetSerial(123);
  return ret;
}

void ExpectErrorWithString(const std::string& error_message,
                           scoped_ptr<dbus::Response> response) {
    dbus::ErrorResponse* error_response =
        dynamic_cast<dbus::ErrorResponse*>(response.get());
    ASSERT_TRUE(error_response != NULL) << "Expected error response, "
                                        << "but didn't get one.";
    dbus::MessageReader reader(error_response);
    ASSERT_TRUE(reader.HasMoreData());
    std::string actual_error_message;
    ASSERT_TRUE(reader.PopString(&actual_error_message));
    ASSERT_FALSE(reader.HasMoreData());
    EXPECT_EQ(error_message, actual_error_message);
}

void ExpectHelloWorld(scoped_ptr<dbus::Response> response) {
  dbus::MessageReader reader(response.get());
  ASSERT_TRUE(reader.HasMoreData());
  string message;
  ASSERT_TRUE(reader.PopString(&message));
  EXPECT_EQ(message, kPingResponse);
}

}  // namespace

namespace peerd {

class ManagerDBusProxyTest: public ::testing::Test {
 public:
  typedef void(Manager::*ManagerDBusHandler)(
      dbus::MethodCall*, dbus::ExportedObject::ResponseSender);

  virtual void SetUp() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);
    bus_ = mock_bus_;  // Takes ownership.
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock exported object.
    exported_object_ = new dbus::MockExportedObject(
        mock_bus_,
        dbus::ObjectPath(kManagerServicePath));
    EXPECT_CALL(*mock_bus_,
                GetExportedObject(dbus::ObjectPath(kManagerServicePath)))
        .Times(1).WillOnce(Return(exported_object_.get()));
    for (const char* method : {kManagerExposeIpService,
                               kManagerPing,
                               kManagerRemoveExposedService,
                               kManagerSetFriendlyName,
                               kManagerSetNote,
                               kManagerStartMonitoring,
                               kManagerStopMonitoring}) {
      EXPECT_CALL(*exported_object_,
                  ExportMethod(kManagerInterface, method, _, _)).Times(1);
    }
    manager_.reset(new MockManager(bus_));
    proxy_ = &manager_->proxy_;
    proxy_->Init(base::Bind(&ManagerDBusProxyTest::InitFinished,
                            base::Unretained(this)));
  }

  virtual void TearDown() {
    EXPECT_CALL(*exported_object_, Unregister()).Times(1);
    proxy_ = nullptr;
    manager_.reset();
    exported_object_ = nullptr;
    mock_bus_ = nullptr;
    bus_ = nullptr;
  }

  MOCK_METHOD1(InitFinished, void(bool));

  scoped_refptr<dbus::Bus> bus_;
  dbus::MockBus* mock_bus_;
  scoped_refptr<dbus::MockExportedObject> exported_object_;
  unique_ptr<MockManager> manager_;
  ManagerDBusProxy* proxy_;
};

TEST_F(ManagerDBusProxyTest, HandleStartMonitoring_NoArgs) {
  auto method_call = MakeMethodCall(kManagerStartMonitoring);
}

TEST_F(ManagerDBusProxyTest, StartMonitoring_ReturnsString) {
  auto method_call = MakeMethodCall(kManagerStartMonitoring);
}

TEST_F(ManagerDBusProxyTest, HandleStopMonitoring_NoArgs) {
  auto method_call = MakeMethodCall(kManagerStopMonitoring);
}

TEST_F(ManagerDBusProxyTest, HandleStopMonitoring_ExtraArgs) {
  auto method_call = MakeMethodCall(kManagerStopMonitoring);
}

TEST_F(ManagerDBusProxyTest, HandleExposeIpService_NoArgs) {
  auto method_call = MakeMethodCall(kManagerExposeIpService);
}

TEST_F(ManagerDBusProxyTest, HandleExposeIpService_OnlyServiceId) {
  auto method_call = MakeMethodCall(kManagerExposeIpService);
}

TEST_F(ManagerDBusProxyTest, HandleExposeIpService_MalformedIps) {
  auto method_call = MakeMethodCall(kManagerExposeIpService);
}

TEST_F(ManagerDBusProxyTest, HandleExposeIpService_MissingServiceDict) {
  auto method_call = MakeMethodCall(kManagerExposeIpService);
}

TEST_F(ManagerDBusProxyTest, HandleExposeIpService_MissingOptionsDict) {
  auto method_call = MakeMethodCall(kManagerExposeIpService);
}

TEST_F(ManagerDBusProxyTest, HandleExposeIpService_ReturnsServiceToken) {
  auto method_call = MakeMethodCall(kManagerExposeIpService);
}

TEST_F(ManagerDBusProxyTest, HandleExposeIpService_ExtraArgs) {
  auto method_call = MakeMethodCall(kManagerExposeIpService);
}

TEST_F(ManagerDBusProxyTest, HandleRemoveExposedService_NoArgs) {
  auto method_call = MakeMethodCall(kManagerRemoveExposedService);
}

TEST_F(ManagerDBusProxyTest, HandleRemoveExposedService_ExtraArgs) {
  auto method_call = MakeMethodCall(kManagerRemoveExposedService);
}

TEST_F(ManagerDBusProxyTest, HandleSetFriendlyName_NoArgs) {
  auto method_call = MakeMethodCall(kManagerSetFriendlyName);
}

TEST_F(ManagerDBusProxyTest, HandleSetFriendlyName_ExtraArgs) {
  auto method_call = MakeMethodCall(kManagerSetFriendlyName);
}

TEST_F(ManagerDBusProxyTest, HandleSetNote_NoArgs) {
  auto method_call = MakeMethodCall(kManagerSetNote);
}

TEST_F(ManagerDBusProxyTest, HandleSetNote_ExtraArgs) {
  auto method_call = MakeMethodCall(kManagerSetNote);
}

TEST_F(ManagerDBusProxyTest, HandlePing_ReturnsHelloWorld) {
  auto method_call = MakeMethodCall(kManagerPing);
  proxy_->HandlePing(method_call.get(), base::Bind(&ExpectHelloWorld));
}

TEST_F(ManagerDBusProxyTest, HandlePing_WithArgs) {
  auto method_call = MakeMethodCall(kManagerPing);
  dbus::MessageWriter writer(method_call.get());
  writer.AppendString("Ping takes no args");
  proxy_->HandlePing(
      method_call.get(),
      base::Bind(&ExpectErrorWithString, kErrorTooManyParameters));
}

}  // namespace peerd
