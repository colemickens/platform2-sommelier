// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/exported_object_manager.h"

#include <base/bind.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

namespace dbus_utils {

namespace {

const dbus::ObjectPath kTestPath(std::string("/test/om_path"));
const dbus::ObjectPath kClaimedTestPath(std::string("/test/claimed_path"));
const std::string kClaimedInterface("claimed.interface");
const std::string kTestPropertyName("PropertyName");
const std::string kTestPropertyValue("PropertyValue");

void WriteTestPropertyDict(dbus::MessageWriter* writer) {
  dbus::MessageWriter all_properties(nullptr);
  dbus::MessageWriter each_property(nullptr);
  writer->OpenArray("{sv}", &all_properties);
  all_properties.OpenDictEntry(&each_property);
  each_property.AppendString(kTestPropertyName);
  each_property.AppendVariantOfString(kTestPropertyValue);
  all_properties.CloseContainer(&each_property);
  writer->CloseContainer(&all_properties);
}

void ReadTestPropertyDict(dbus::MessageReader* reader) {
  dbus::MessageReader all_properties(nullptr);
  dbus::MessageReader each_property(nullptr);
  ASSERT_TRUE(reader->PopArray(&all_properties));
  ASSERT_TRUE(all_properties.PopDictEntry(&each_property));
  std::string property_name;
  std::string property_value;
  ASSERT_TRUE(each_property.PopString(&property_name));
  ASSERT_TRUE(each_property.PopVariantOfString(&property_value));
  EXPECT_FALSE(each_property.HasMoreData());
  EXPECT_FALSE(all_properties.HasMoreData());
  EXPECT_EQ(property_name, kTestPropertyName);
  EXPECT_EQ(property_value, kTestPropertyValue);
}

void VerifyInterfaceClaimSignal(dbus::Signal* signal) {
  EXPECT_EQ(signal->GetInterface(),
            std::string(dbus::kObjectManagerInterface));
  EXPECT_EQ(signal->GetMember(),
            std::string(dbus::kObjectManagerInterfacesAdded));
  //   org.freedesktop.DBus.ObjectManager.InterfacesAdded (
  //       OBJPATH object_path,
  //       DICT<STRING,DICT<STRING,VARIANT>> interfaces_and_properties);
  dbus::MessageReader reader(signal);
  dbus::MessageReader all_interfaces(nullptr);
  dbus::MessageReader each_interface(nullptr);
  dbus::ObjectPath path;
  ASSERT_TRUE(reader.PopObjectPath(&path));
  ASSERT_TRUE(reader.PopArray(&all_interfaces));
  ASSERT_TRUE(all_interfaces.PopDictEntry(&each_interface));
  std::string interface_name;
  ASSERT_TRUE(each_interface.PopString(&interface_name));
  ReadTestPropertyDict(&each_interface);
  EXPECT_FALSE(each_interface.HasMoreData());
  EXPECT_FALSE(all_interfaces.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_EQ(interface_name, kClaimedInterface);
  EXPECT_EQ(path, kClaimedTestPath);
}

void VerifyInterfaceDropSignal(dbus::Signal* signal) {
  EXPECT_EQ(signal->GetInterface(),
            std::string(dbus::kObjectManagerInterface));
  EXPECT_EQ(signal->GetMember(),
            std::string(dbus::kObjectManagerInterfacesRemoved));
  //   org.freedesktop.DBus.ObjectManager.InterfacesRemoved (
  //       OBJPATH object_path, ARRAY<STRING> interfaces);
  dbus::MessageReader reader(signal);
  dbus::MessageReader each_interface(nullptr);
  dbus::ObjectPath path;
  ASSERT_TRUE(reader.PopObjectPath(&path));
  ASSERT_TRUE(reader.PopArray(&each_interface));
  std::string interface_name;
  ASSERT_TRUE(each_interface.PopString(&interface_name));
  EXPECT_FALSE(each_interface.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_EQ(interface_name, kClaimedInterface);
  EXPECT_EQ(path, kClaimedTestPath);
}

}  // namespace

class ExportedObjectManagerTest: public ::testing::Test {
 public:
  virtual void SetUp() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock exported object.
    mock_exported_object_ = new dbus::MockExportedObject(
        bus_.get(), kTestPath);
    EXPECT_CALL(*bus_, GetExportedObject(kTestPath))
        .Times(1).WillOnce(Return(mock_exported_object_.get()));
    om_.reset(new ExportedObjectManager(bus_.get(), kTestPath));
    property_writer_ = base::Bind(&WriteTestPropertyDict);
    response_storer_ = base::Bind(&ExportedObjectManagerTest::StoreResponse,
                                  base::Unretained(this));
  }

  void StoreResponse(scoped_ptr<dbus::Response> method_response) {
    last_response_.reset(method_response.release());
  }

  void CallHandleGetManagedObjects(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender sender) {
    om_->HandleGetManagedObjects(method_call, response_storer_);
  }

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  scoped_ptr<ExportedObjectManager> om_;
  ExportedObjectManager::PropertyWriter property_writer_;
  dbus::ExportedObject::ResponseSender response_storer_;
  scoped_ptr<dbus::Response> last_response_;
};

TEST_F(ExportedObjectManagerTest, ClaimInterfaceSendsSignals) {
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .Times(1).WillOnce(Invoke(&VerifyInterfaceClaimSignal));
  om_->ClaimInterface(kClaimedTestPath, kClaimedInterface, property_writer_);
}

TEST_F(ExportedObjectManagerTest, ReleaseInterfaceSendsSignals) {
  InSequence dummy;
  EXPECT_CALL(*mock_exported_object_, SendSignal(_)).Times(1);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .Times(1).WillOnce(Invoke(&VerifyInterfaceDropSignal));
  om_->ClaimInterface(kClaimedTestPath, kClaimedInterface, property_writer_);
  om_->ReleaseInterface(kClaimedTestPath, kClaimedInterface);
}

TEST_F(ExportedObjectManagerTest, GetManagedObjectsResponseEmptyCorrectness) {
  dbus::MethodCall method_call(dbus::kObjectManagerInterface,
                               dbus::kObjectManagerGetManagedObjects);
  method_call.SetSerial(123);
  CallHandleGetManagedObjects(&method_call, response_storer_);
  dbus::MessageReader reader(last_response_.get());
  dbus::MessageReader all_paths(nullptr);
  ASSERT_TRUE(reader.PopArray(&all_paths));
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(ExportedObjectManagerTest, GetManagedObjectsResponseCorrectness) {
  // org.freedesktop.DBus.ObjectManager.GetManagedObjects (
  //     out DICT<OBJPATH,
  //              DICT<STRING,
  //                   DICT<STRING,VARIANT>>> )
  dbus::MethodCall method_call(dbus::kObjectManagerInterface,
                               dbus::kObjectManagerGetManagedObjects);
  method_call.SetSerial(123);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_)).Times(1);
  om_->ClaimInterface(kClaimedTestPath, kClaimedInterface, property_writer_);
  CallHandleGetManagedObjects(&method_call, response_storer_);
  dbus::MessageReader reader(last_response_.get());
  dbus::MessageReader all_paths(nullptr);
  dbus::MessageReader each_path(nullptr);
  dbus::MessageReader all_interfaces(nullptr);
  dbus::MessageReader each_interface(nullptr);
  ASSERT_TRUE(reader.PopArray(&all_paths));
  ASSERT_TRUE(all_paths.PopDictEntry(&each_path));
  dbus::ObjectPath path;
  ASSERT_TRUE(each_path.PopObjectPath(&path));
  ASSERT_TRUE(each_path.PopArray(&all_interfaces));
  ASSERT_TRUE(all_interfaces.PopDictEntry(&each_interface));
  std::string interface_name;
  ASSERT_TRUE(each_interface.PopString(&interface_name));
  ReadTestPropertyDict(&each_interface);
  EXPECT_FALSE(each_interface.HasMoreData());
  EXPECT_FALSE(all_interfaces.HasMoreData());
  EXPECT_FALSE(each_path.HasMoreData());
  EXPECT_FALSE(all_paths.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_EQ(path, kClaimedTestPath);
  EXPECT_EQ(interface_name, kClaimedInterface);
}

}  // namespace dbus_utils

}  // namespace chromeos
