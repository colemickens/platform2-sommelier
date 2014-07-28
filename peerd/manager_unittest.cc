// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <string>

#include <base/bind.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

#include "peerd/dbus_constants.h"

using peerd::dbus_constants::kManagerInterface;
using peerd::dbus_constants::kManagerPing;
using peerd::dbus_constants::kManagerServicePath;
using std::unique_ptr;
using testing::AnyNumber;
using testing::Return;

namespace {

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
    EXPECT_EQ(actual_error_message, error_message);
}

void ExpectHelloWorld(scoped_ptr<dbus::Response> response) {
  dbus::MessageReader reader(response.get());
  ASSERT_TRUE(reader.HasMoreData());
  std::string message;
  ASSERT_TRUE(reader.PopString(&message));
  EXPECT_EQ(message, "Hello world!");
}

}  // namespace

namespace peerd {

class ManagerTest: public ::testing::Test {
 public:
  virtual void SetUp() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock exported object.
    exported_object_ = new dbus::MockExportedObject(
        bus_.get(),
        dbus::ObjectPath(kManagerServicePath));
    EXPECT_CALL(*bus_, GetExportedObject(dbus::ObjectPath(kManagerServicePath)))
        .Times(1).WillOnce(Return(exported_object_.get()));
    manager_.reset(new peerd::Manager(scoped_refptr<dbus::Bus>(bus_.get())));
  }

  virtual void TearDown() {
    EXPECT_CALL(*exported_object_, Unregister()).Times(1);
  }

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> exported_object_;
  unique_ptr<peerd::Manager> manager_;
};

TEST_F(ManagerTest, PingReturnsHelloWorld) {
  dbus::MethodCall method_call(kManagerInterface, kManagerPing);
  method_call.SetSerial(123);
  manager_->HandlePing(&method_call, base::Bind(&ExpectHelloWorld));
}

TEST_F(ManagerTest, RejectPingWithArgs) {
  dbus::MethodCall method_call(kManagerInterface, kManagerPing);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("Ping takes no args");
  manager_->HandlePing(
      &method_call,
      base::Bind(&ExpectErrorWithString, "Too many parameters to Ping"));
}


}  // namespace peerd
