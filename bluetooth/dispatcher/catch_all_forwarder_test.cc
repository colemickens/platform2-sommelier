// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/catch_all_forwarder.h"

#include <memory>

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>

#include "bluetooth/dispatcher/test_helper.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArgPointee;

namespace bluetooth {

namespace {

constexpr char kRootPath[] = "/";
constexpr char kServiceDestination[] = ":1.100";

constexpr char kTestInterface[] = "org.test.TestInterface";
constexpr char kTestMethod[] = "TestMethod";
constexpr char kTestPath[] = "/org/test/object";
constexpr char kTestSender[] = ":1.1";

constexpr char kTestMethodCallString[] = "The Method Call";
constexpr char kTestResponseString[] = "The Response";

constexpr char kTestErrorName[] = "org.example.Error";
constexpr char kTestErrorResponseString[] = "Example Error";

constexpr int kTestSerial = 10;

}  // namespace

class CatchAllForwarderTest : public ::testing::Test {
 public:
  void SetUp() override {
    from_bus_ = new dbus::MockBus(dbus::Bus::Options());
    to_bus_ = new dbus::MockBus(dbus::Bus::Options());
    EXPECT_CALL(*from_bus_, AssertOnDBusThread()).Times(AnyNumber());
    catch_all_forwarder_ = std::make_unique<CatchAllForwarder>(
        from_bus_, to_bus_, kServiceDestination);
  }

  void StubHandleTestMethod(dbus::MethodCall* method_call,
                            int timeout_ms,
                            dbus::ObjectProxy::ResponseCallback callback,
                            dbus::ObjectProxy::ErrorCallback error_callback) {
    StubHandleMethod(kTestInterface, kTestMethod, kTestMethodCallString,
                     kTestResponseString, "" /* error_name */,
                     "" /* error_message */, method_call, timeout_ms, callback,
                     error_callback);
  }

  void StubHandleTestMethodError(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseCallback callback,
      dbus::ObjectProxy::ErrorCallback error_callback) {
    StubHandleMethod(kTestInterface, kTestMethod, kTestMethodCallString,
                     kTestResponseString, kTestErrorName,
                     kTestErrorResponseString, method_call, timeout_ms,
                     callback, error_callback);
  }

 protected:
  scoped_refptr<dbus::MockBus> from_bus_;
  scoped_refptr<dbus::MockBus> to_bus_;
  std::unique_ptr<CatchAllForwarder> catch_all_forwarder_;
};

TEST_F(CatchAllForwarderTest, ForwardMethod) {
  DBusObjectPathVTable vtable;
  EXPECT_CALL(*from_bus_, IsConnected()).WillRepeatedly(Return(true));
  EXPECT_CALL(*from_bus_,
              TryRegisterFallback(dbus::ObjectPath(kRootPath), _, _, _))
      .WillOnce(DoAll(SaveArgPointee<1>(&vtable), Return(true)));

  catch_all_forwarder_->Init();
  ASSERT_TRUE(vtable.message_function != nullptr);

  dbus::MethodCall method_call(kTestInterface, kTestMethod);
  method_call.SetPath(dbus::ObjectPath(kTestPath));
  method_call.SetSender(kTestSender);
  method_call.SetSerial(kTestSerial);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kTestMethodCallString);

  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      from_bus_.get(), kServiceDestination, dbus::ObjectPath(kTestPath));
  EXPECT_CALL(*to_bus_,
              GetObjectProxy(kServiceDestination, dbus::ObjectPath(kTestPath)))
      .WillOnce(Return(object_proxy.get()));
  EXPECT_CALL(*object_proxy, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this, &CatchAllForwarderTest::StubHandleTestMethod));

  DBusMessage* raw_response = nullptr;
  EXPECT_CALL(*from_bus_, Send(_, _))
      .WillOnce(Invoke([&raw_response](DBusMessage* msg, uint32_t* serial) {
        raw_response = dbus_message_copy(msg);
      }));
  vtable.message_function(nullptr /* connection */, method_call.raw_message(),
                          catch_all_forwarder_.get());

  ASSERT_NE(nullptr, raw_response);
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromRawMessage(raw_response);

  std::string response_string;
  dbus::MessageReader reader(response.get());
  reader.PopString(&response_string);
  // Check that the response is the forwarded response of the stub method
  // handler.
  EXPECT_EQ(kTestSender, response->GetDestination());
  EXPECT_EQ(kTestSerial, response->GetReplySerial());
  EXPECT_EQ(kTestResponseString, response_string);

  // Test message forwarding again, but this time the method returns error.
  EXPECT_CALL(*to_bus_,
              GetObjectProxy(kServiceDestination, dbus::ObjectPath(kTestPath)))
      .WillOnce(Return(object_proxy.get()));
  EXPECT_CALL(*object_proxy, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(
          Invoke(this, &CatchAllForwarderTest::StubHandleTestMethodError));
  EXPECT_CALL(*from_bus_, Send(_, _))
      .WillOnce(Invoke([&raw_response](DBusMessage* msg, uint32_t* serial) {
        raw_response = dbus_message_copy(msg);
      }));
  vtable.message_function(nullptr /* connection */, method_call.raw_message(),
                          catch_all_forwarder_.get());
  ASSERT_NE(nullptr, raw_response);
  response = dbus::ErrorResponse::FromRawMessage(raw_response);
  dbus::MessageReader error_reader(response.get());
  error_reader.PopString(&response_string);
  // Check that the response is the forwarded response of the stub method
  // handler.
  EXPECT_EQ(kTestSender, response->GetDestination());
  EXPECT_EQ(kTestSerial, response->GetReplySerial());
  EXPECT_EQ(kTestErrorName, response->GetErrorName());
  EXPECT_EQ(kTestErrorResponseString, response_string);
}

}  // namespace bluetooth
