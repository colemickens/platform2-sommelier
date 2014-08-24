// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/dbus_object.h>

#include <memory>

#include <base/bind.h>
#include <dbus/message.h>
#include <dbus/property.h>
#include <dbus/object_path.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;

namespace chromeos {
namespace dbus_utils {

namespace {

const char kTestInterface1[] = "org.chromium.Test.MathInterface";
const char kTestMethod_Add[] = "Add";
const char kTestMethod_Negate[] = "Negate";
const char kTestMethod_Positive[] = "Positive";

const char kTestInterface2[] = "org.chromium.Test.StringInterface";
const char kTestMethod_StrLen[] = "StrLen";

const char kTestInterface3[] = "org.chromium.Test.NoOpInterface";
const char kTestMethod_NoOp[] = "NoOp";

void NoAction(bool all_succeeded) {}

struct Calc {
  int Add(ErrorPtr* error, int x, int y) { return x + y; }
  int Negate(ErrorPtr* error, int x) { return -x; }
  double Positive(ErrorPtr* error, double x) {
    if (x >= 0.0)
      return x;
    Error::AddTo(error, "test", "not_positive", "Negative value passed in");
    return 0.0;
  }
};

int StrLen(ErrorPtr* error, const std::string& str) {
  return str.size();
}

void NoOp(ErrorPtr* error) {}

}  // namespace

class DBusObjectTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock exported object.
    const dbus::ObjectPath kMethodsExportedOnPath(std::string("/export"));
    mock_exported_object_ = new dbus::MockExportedObject(
        bus_.get(), kMethodsExportedOnPath);
    EXPECT_CALL(*bus_, GetExportedObject(kMethodsExportedOnPath))
        .Times(AnyNumber()).WillRepeatedly(Return(mock_exported_object_.get()));
    EXPECT_CALL(*mock_exported_object_,
                ExportMethod(_, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock_exported_object_, Unregister()).Times(1);

    dbus_object_ = std::unique_ptr<DBusObject>(
        new DBusObject(nullptr, bus_, kMethodsExportedOnPath));

    DBusInterface* itf1 = dbus_object_->AddOrGetInterface(kTestInterface1);
    itf1->AddMethodHandler(kTestMethod_Add,
                           base::Unretained(&calc_), &Calc::Add);
    itf1->AddMethodHandler(kTestMethod_Negate,
                           base::Unretained(&calc_), &Calc::Negate);
    itf1->AddMethodHandler(kTestMethod_Positive,
                           base::Unretained(&calc_), &Calc::Positive);
    DBusInterface* itf2 = dbus_object_->AddOrGetInterface(kTestInterface2);
    itf2->AddMethodHandler(kTestMethod_StrLen, StrLen);
    DBusInterface* itf3 = dbus_object_->AddOrGetInterface(kTestInterface3);
    base::Callback<void(ErrorPtr*)> noop_callback = base::Bind(NoOp);
    itf3->AddMethodHandler(kTestMethod_NoOp, noop_callback);

    dbus_object_->RegisterAsync(base::Bind(NoAction));
  }

  void ExpectError(dbus::Response* response, const std::string& expected_code) {
    EXPECT_EQ(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
    EXPECT_EQ(expected_code, response->GetErrorName());
  }

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  std::unique_ptr<DBusObject> dbus_object_;
  Calc calc_;
};

TEST_F(DBusObjectTest, Add) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Add);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(2);
  writer.AppendInt32(3);
  auto response = CallMethod(*dbus_object_, &method_call);
  dbus::MessageReader reader(response.get());
  int result;
  ASSERT_TRUE(reader.PopInt32(&result));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(5, result);
}

TEST_F(DBusObjectTest, Negate) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Negate);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(98765);
  auto response = CallMethod(*dbus_object_, &method_call);
  dbus::MessageReader reader(response.get());
  int result;
  ASSERT_TRUE(reader.PopInt32(&result));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(-98765, result);
}

TEST_F(DBusObjectTest, PositiveSuccess) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Positive);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(17.5);
  auto response = CallMethod(*dbus_object_, &method_call);
  dbus::MessageReader reader(response.get());
  double result;
  ASSERT_TRUE(reader.PopDouble(&result));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_DOUBLE_EQ(17.5, result);
}

TEST_F(DBusObjectTest, PositiveFailure) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Positive);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(-23.2);
  auto response = CallMethod(*dbus_object_, &method_call);
  ExpectError(response.get(), DBUS_ERROR_FAILED);
}

TEST_F(DBusObjectTest, StrLen0) {
  dbus::MethodCall method_call(kTestInterface2, kTestMethod_StrLen);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("");
  auto response = CallMethod(*dbus_object_, &method_call);
  dbus::MessageReader reader(response.get());
  int result;
  ASSERT_TRUE(reader.PopInt32(&result));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(0, result);
}

TEST_F(DBusObjectTest, StrLen4) {
  dbus::MethodCall method_call(kTestInterface2, kTestMethod_StrLen);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("test");
  auto response = CallMethod(*dbus_object_, &method_call);
  dbus::MessageReader reader(response.get());
  int result;
  ASSERT_TRUE(reader.PopInt32(&result));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(4, result);
}

TEST_F(DBusObjectTest, NoOp) {
  dbus::MethodCall method_call(kTestInterface3, kTestMethod_NoOp);
  method_call.SetSerial(123);
  auto response = CallMethod(*dbus_object_, &method_call);
  dbus::MessageReader reader(response.get());
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(DBusObjectTest, TooFewParams) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Add);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(2);
  auto response = CallMethod(*dbus_object_, &method_call);
  ExpectError(response.get(), DBUS_ERROR_INVALID_ARGS);
}

TEST_F(DBusObjectTest, TooManyParams) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Add);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(1);
  writer.AppendInt32(2);
  writer.AppendInt32(3);
  auto response = CallMethod(*dbus_object_, &method_call);
  ExpectError(response.get(), DBUS_ERROR_INVALID_ARGS);
}

TEST_F(DBusObjectTest, ParamTypeMismatch) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Add);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(1);
  writer.AppendBool(false);
  auto response = CallMethod(*dbus_object_, &method_call);
  ExpectError(response.get(), DBUS_ERROR_INVALID_ARGS);
}

TEST_F(DBusObjectTest, ParamAsVariant) {
  dbus::MethodCall method_call(kTestInterface1, kTestMethod_Add);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendVariantOfInt32(10);
  writer.AppendVariantOfInt32(3);
  auto response = CallMethod(*dbus_object_, &method_call);
  dbus::MessageReader reader(response.get());
  int result;
  ASSERT_TRUE(reader.PopInt32(&result));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(13, result);
}

TEST_F(DBusObjectTest, UnknownMethod) {
  dbus::MethodCall method_call(kTestInterface2, kTestMethod_Add);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(1);
  writer.AppendBool(false);
  auto response = CallMethod(*dbus_object_, &method_call);
  ExpectError(response.get(), DBUS_ERROR_UNKNOWN_METHOD);
}

}  // namespace dbus_utils
}  // namespace chromeos
