// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/exported_property_set.h"

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/bind.h>
#include <dbus/message.h>
#include <dbus/property.h>
#include <dbus/object_path.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chromeos/dbus/dbus_object.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;

namespace chromeos {

namespace dbus_utils {

namespace {

const char kBoolPropName[] = "BoolProp";
const char kUint8PropName[] = "Uint8Prop";
const char kInt16PropName[] = "Int16Prop";
const char kUint16PropName[] = "Uint16Prop";
const char kInt32PropName[] = "Int32Prop";
const char kUint32PropName[] = "Uint32Prop";
const char kInt64PropName[] = "Int64Prop";
const char kUint64PropName[] = "Uint64Prop";
const char kDoublePropName[] = "DoubleProp";
const char kStringPropName[] = "StringProp";
const char kPathPropName[] = "PathProp";
const char kStringListPropName[] = "StringListProp";
const char kPathListPropName[] = "PathListProp";
const char kUint8ListPropName[] = "Uint8ListProp";

const char kTestInterface1[] = "org.chromium.TestInterface1";
const char kTestInterface2[] = "org.chromium.TestInterface2";
const char kTestInterface3[] = "org.chromium.TestInterface3";

const std::string kTestString("lies");
const dbus::ObjectPath kMethodsExportedOnPath(std::string("/export"));
const dbus::ObjectPath kTestObjectPathInit(std::string("/path_init"));
const dbus::ObjectPath kTestObjectPathUpdate(std::string("/path_update"));

void NoAction(bool all_succeeded) {}

}  // namespace

class ExportedPropertySetTest : public ::testing::Test {
 public:
  struct Properties {
   public:
    ExportedProperty<bool> bool_prop_;
    ExportedProperty<uint8_t> uint8_prop_;
    ExportedProperty<int16_t> int16_prop_;
    ExportedProperty<uint16_t> uint16_prop_;
    ExportedProperty<int32_t> int32_prop_;
    ExportedProperty<uint32_t> uint32_prop_;
    ExportedProperty<int64_t> int64_prop_;
    ExportedProperty<uint64_t> uint64_prop_;
    ExportedProperty<double> double_prop_;
    ExportedProperty<std::string> string_prop_;
    ExportedProperty<dbus::ObjectPath> path_prop_;
    ExportedProperty<std::vector<std::string>> stringlist_prop_;
    ExportedProperty<std::vector<dbus::ObjectPath>> pathlist_prop_;
    ExportedProperty<std::vector<uint8_t>> uint8list_prop_;

    Properties(scoped_refptr<dbus::Bus> bus, const dbus::ObjectPath& path)
        : dbus_object_(nullptr, bus, path) {
      // The empty string is not a valid value for an ObjectPath.
      path_prop_.SetValue(kTestObjectPathInit);
      DBusInterface* itf1 = dbus_object_.AddOrGetInterface(kTestInterface1);
      itf1->AddProperty(kBoolPropName, &bool_prop_);
      itf1->AddProperty(kUint8PropName, &uint8_prop_);
      itf1->AddProperty(kInt16PropName, &int16_prop_);
      // I chose this weird grouping because N=2 is about all the permutations
      // of GetAll that I want to anticipate.
      DBusInterface* itf2 = dbus_object_.AddOrGetInterface(kTestInterface2);
      itf2->AddProperty(kUint16PropName, &uint16_prop_);
      itf2->AddProperty(kInt32PropName, &int32_prop_);
      DBusInterface* itf3 = dbus_object_.AddOrGetInterface(kTestInterface3);
      itf3->AddProperty(kUint32PropName, &uint32_prop_);
      itf3->AddProperty(kInt64PropName, &int64_prop_);
      itf3->AddProperty(kUint64PropName, &uint64_prop_);
      itf3->AddProperty(kDoublePropName, &double_prop_);
      itf3->AddProperty(kStringPropName, &string_prop_);
      itf3->AddProperty(kPathPropName, &path_prop_);
      itf3->AddProperty(kStringListPropName, &stringlist_prop_);
      itf3->AddProperty(kPathListPropName, &pathlist_prop_);
      itf3->AddProperty(kUint8ListPropName, &uint8list_prop_);
      dbus_object_.RegisterAsync(base::Bind(NoAction));
    }
    virtual ~Properties() {}

    scoped_ptr<dbus::Response> CallMethod(
        dbus::MethodCall* method_call) {
      DBusInterfaceMethodHandler* handler = dbus_object_.FindMethodHandler(
          method_call->GetInterface(), method_call->GetMember());
      std::unique_ptr<dbus::Response> response;
      if (!handler) {
        response = CreateDBusErrorResponse(
            method_call,
            "org.freedesktop.DBus.Error.UnknownMethod",
            "Unknown method");
      } else {
        response = handler->HandleMethod(method_call);
      }
      return scoped_ptr<dbus::Response>(response.release());
    }

    DBusObject dbus_object_;
  };

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock exported object.
    mock_exported_object_ = new dbus::MockExportedObject(
        bus_.get(), kMethodsExportedOnPath);
    EXPECT_CALL(*bus_, GetExportedObject(kMethodsExportedOnPath))
        .Times(1).WillOnce(Return(mock_exported_object_.get()));

    EXPECT_CALL(*mock_exported_object_,
                ExportMethod(dbus::kPropertiesInterface, _, _, _)).Times(3);
    p_.reset(new Properties(bus_, kMethodsExportedOnPath));
  }

  void TearDown() override {
    EXPECT_CALL(*mock_exported_object_, Unregister()).Times(1);
  }

  void AssertMethodReturnsError(dbus::MethodCall* method_call) {
    method_call->SetSerial(123);
    auto response = p_->CallMethod(method_call);
    ASSERT_NE(dynamic_cast<dbus::ErrorResponse*>(response.get()), nullptr);
  }

  scoped_ptr<dbus::Response> GetPropertyOnInterface(
      const std::string& interface_name, const std::string& property_name) {
    dbus::MethodCall method_call(dbus::kPropertiesInterface,
                                 dbus::kPropertiesGet);
    method_call.SetSerial(123);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(interface_name);
    writer.AppendString(property_name);
    return p_->CallMethod(&method_call);
  }

  scoped_ptr<dbus::Response> last_response_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  scoped_ptr<Properties> p_;
};

TEST_F(ExportedPropertySetTest, UpdateNotifications) {
  EXPECT_CALL(*mock_exported_object_, SendSignal(_)).Times(14);
  p_->bool_prop_.SetValue(true);
  p_->uint8_prop_.SetValue(1);
  p_->int16_prop_.SetValue(1);
  p_->uint16_prop_.SetValue(1);
  p_->int32_prop_.SetValue(1);
  p_->uint32_prop_.SetValue(1);
  p_->int64_prop_.SetValue(1);
  p_->uint64_prop_.SetValue(1);
  p_->double_prop_.SetValue(1.0);
  p_->string_prop_.SetValue(kTestString);
  p_->path_prop_.SetValue(kTestObjectPathUpdate);
  p_->stringlist_prop_.SetValue({kTestString});
  p_->pathlist_prop_.SetValue({kTestObjectPathUpdate});
  p_->uint8list_prop_.SetValue({1});
}

TEST_F(ExportedPropertySetTest, UpdateToSameValue) {
  EXPECT_CALL(*mock_exported_object_, SendSignal(_)).Times(1);
  p_->bool_prop_.SetValue(true);
  p_->bool_prop_.SetValue(true);
}

TEST_F(ExportedPropertySetTest, GetAllNoArgs) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGetAll);
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetAllInvalidInterface) {
  dbus::MethodCall method_call(
      dbus::kPropertiesInterface, dbus::kPropertiesGetAll);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("org.chromium.BadInterface");
  auto response = p_->CallMethod(&method_call);
  dbus::MessageReader response_reader(response.get());
  dbus::MessageReader dict_reader(nullptr);
  ASSERT_TRUE(response_reader.PopArray(&dict_reader));
  // The response should just be a an empty array, since there are no properties
  // on this interface.  The spec doesn't say much about error conditions here,
  // so I'm going to assume this is a valid implementation.
  ASSERT_FALSE(dict_reader.HasMoreData());
  ASSERT_FALSE(response_reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetAllExtraArgs) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGetAll);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kTestInterface1);
  writer.AppendString(kTestInterface1);
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetAllCorrectness) {
  dbus::MethodCall method_call(
      dbus::kPropertiesInterface, dbus::kPropertiesGetAll);
  method_call.SetSerial(123);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kTestInterface2);
  auto response = p_->CallMethod(&method_call);
  dbus::MessageReader response_reader(response.get());
  dbus::MessageReader dict_reader(nullptr);
  dbus::MessageReader entry_reader(nullptr);
  ASSERT_TRUE(response_reader.PopArray(&dict_reader));
  ASSERT_TRUE(dict_reader.PopDictEntry(&entry_reader));
  std::string property_name;
  ASSERT_TRUE(entry_reader.PopString(&property_name));
  uint16_t value16;
  int32_t value32;
  if (property_name.compare(kUint16PropName) == 0) {
    ASSERT_TRUE(entry_reader.PopVariantOfUint16(&value16));
    ASSERT_FALSE(entry_reader.HasMoreData());
    ASSERT_TRUE(dict_reader.PopDictEntry(&entry_reader));
    ASSERT_TRUE(entry_reader.PopString(&property_name));
    ASSERT_EQ(property_name.compare(kInt32PropName), 0);
    ASSERT_TRUE(entry_reader.PopVariantOfInt32(&value32));
  } else {
    ASSERT_EQ(property_name.compare(kInt32PropName), 0);
    ASSERT_TRUE(entry_reader.PopVariantOfInt32(&value32));
    ASSERT_FALSE(entry_reader.HasMoreData());
    ASSERT_TRUE(dict_reader.PopDictEntry(&entry_reader));
    ASSERT_TRUE(entry_reader.PopString(&property_name));
    ASSERT_EQ(property_name.compare(kUint16PropName), 0);
    ASSERT_TRUE(entry_reader.PopVariantOfUint16(&value16));
  }
  ASSERT_FALSE(entry_reader.HasMoreData());
  ASSERT_FALSE(dict_reader.HasMoreData());
  ASSERT_FALSE(response_reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetNoArgs) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGet);
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetInvalidInterface) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGet);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("org.chromium.BadInterface");
  writer.AppendString(kInt16PropName);
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetBadPropertyName) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGet);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kTestInterface1);
  writer.AppendString("IAmNotAProperty");
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetPropIfMismatch) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGet);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kTestInterface1);
  writer.AppendString(kStringPropName);
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetNoPropertyName) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGet);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kTestInterface1);
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetExtraArgs) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesGet);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kTestInterface1);
  writer.AppendString(kBoolPropName);
  writer.AppendString("Extra param");
  AssertMethodReturnsError(&method_call);
}

TEST_F(ExportedPropertySetTest, GetWorksWithBool) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface1, kBoolPropName);
  dbus::MessageReader reader(response.get());
  bool value;
  ASSERT_TRUE(reader.PopVariantOfBool(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithUint8) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface1, kUint8PropName);
  dbus::MessageReader reader(response.get());
  uint8_t value;
  ASSERT_TRUE(reader.PopVariantOfByte(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithInt16) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface1, kInt16PropName);
  dbus::MessageReader reader(response.get());
  int16_t value;
  ASSERT_TRUE(reader.PopVariantOfInt16(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithUint16) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface2, kUint16PropName);
  dbus::MessageReader reader(response.get());
  uint16_t value;
  ASSERT_TRUE(reader.PopVariantOfUint16(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithInt32) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface2, kInt32PropName);
  dbus::MessageReader reader(response.get());
  int32_t value;
  ASSERT_TRUE(reader.PopVariantOfInt32(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithUint32) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kUint32PropName);
  dbus::MessageReader reader(response.get());
  uint32_t value;
  ASSERT_TRUE(reader.PopVariantOfUint32(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithInt64) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kInt64PropName);
  dbus::MessageReader reader(response.get());
  int64_t value;
  ASSERT_TRUE(reader.PopVariantOfInt64(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithUint64) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kUint64PropName);
  dbus::MessageReader reader(response.get());
  uint64_t value;
  ASSERT_TRUE(reader.PopVariantOfUint64(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithDouble) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kDoublePropName);
  dbus::MessageReader reader(response.get());
  double value;
  ASSERT_TRUE(reader.PopVariantOfDouble(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithString) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kStringPropName);
  dbus::MessageReader reader(response.get());
  std::string value;
  ASSERT_TRUE(reader.PopVariantOfString(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithPath) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kPathPropName);
  dbus::MessageReader reader(response.get());
  dbus::ObjectPath value;
  ASSERT_TRUE(reader.PopVariantOfObjectPath(&value));
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithStringList) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kStringListPropName);
  dbus::MessageReader reader(response.get());
  dbus::MessageReader variant_reader(nullptr);
  std::vector<std::string> value;
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  ASSERT_TRUE(variant_reader.PopArrayOfStrings(&value));
  ASSERT_FALSE(variant_reader.HasMoreData());
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithPathList) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kPathListPropName);
  dbus::MessageReader reader(response.get());
  dbus::MessageReader variant_reader(nullptr);
  std::vector<dbus::ObjectPath> value;
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  ASSERT_TRUE(variant_reader.PopArrayOfObjectPaths(&value));
  ASSERT_FALSE(variant_reader.HasMoreData());
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, GetWorksWithUint8List) {
  scoped_ptr<dbus::Response> response = GetPropertyOnInterface(
      kTestInterface3, kPathListPropName);
  dbus::MessageReader reader(response.get());
  dbus::MessageReader variant_reader(nullptr);
  const uint8_t* buffer;
  size_t buffer_len;
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  // |buffer| remains under the control of the MessageReader.
  ASSERT_TRUE(variant_reader.PopArrayOfBytes(&buffer, &buffer_len));
  ASSERT_FALSE(variant_reader.HasMoreData());
  ASSERT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedPropertySetTest, SetFailsGracefully) {
  dbus::MethodCall method_call(dbus::kPropertiesInterface,
                               dbus::kPropertiesSet);
  method_call.SetSerial(123);
  auto response = p_->CallMethod(&method_call);
  ASSERT_TRUE(
      dynamic_cast<dbus::ErrorResponse*>(response.get()) != nullptr);
}

namespace {

void VerifySignal(dbus::Signal* signal) {
  ASSERT_NE(signal, nullptr);
  std::string interface_name;
  std::string property_name;
  uint8_t value;
  dbus::MessageReader reader(signal);
  dbus::MessageReader array_reader(signal);
  dbus::MessageReader dict_reader(signal);
  ASSERT_TRUE(reader.PopString(&interface_name));
  ASSERT_TRUE(reader.PopArray(&array_reader));
  ASSERT_TRUE(array_reader.PopDictEntry(&dict_reader));
  ASSERT_TRUE(dict_reader.PopString(&property_name));
  ASSERT_TRUE(dict_reader.PopVariantOfByte(&value));
  ASSERT_FALSE(dict_reader.HasMoreData());
  ASSERT_FALSE(array_reader.HasMoreData());
  ASSERT_TRUE(reader.HasMoreData());
  // Read the (empty) list of invalidated property names.
  ASSERT_TRUE(reader.PopArray(&array_reader));
  ASSERT_FALSE(array_reader.HasMoreData());
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(value, 57);
  ASSERT_EQ(property_name, std::string(kUint8PropName));
  ASSERT_EQ(interface_name, std::string(kTestInterface1));
}

}  // namespace

TEST_F(ExportedPropertySetTest, SignalsAreParsable) {
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .Times(1).WillOnce(Invoke(&VerifySignal));
  p_->uint8_prop_.SetValue(57);
}

}  // namespace dbus_utils

}  // namespace chromeos
