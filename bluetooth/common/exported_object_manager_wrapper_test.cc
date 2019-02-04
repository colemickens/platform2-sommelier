// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/bind.h>
#include <brillo/dbus/exported_property_set.h>
#include <brillo/dbus/mock_dbus_object.h>
#include <brillo/dbus/mock_exported_object_manager.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/property.h>
#include <gtest/gtest.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;

namespace bluetooth {

namespace {

constexpr char kTestInterfaceName1[] = "org.example.Interface1";
constexpr char kTestInterfaceName2[] = "org.example.Interface2";
constexpr char kTestObjectPath[] = "/org/example/Object";
constexpr char kTestObjectManagerPath[] = "/";
constexpr char kTestPropertyName[] = "SomeProperty";
constexpr char kTestServiceName[] = "org.example.Default";
constexpr char kTestMethodName1[] = "org.example.Method1";

// Matcher to compare a D-Bus signal for properties changed.
MATCHER_P3(PropertySignalEq,
           expected_property_interface,
           expected_property_name,
           expected_property_value,
           "") {
  std::string property_interface;
  brillo::VariantDictionary dict;
  dbus::MessageReader reader(arg);

  // Try to read the message payload.
  if (!brillo::dbus_utils::PopValueFromReader(&reader, &property_interface))
    return false;
  if (!brillo::dbus_utils::PopValueFromReader(&reader, &dict))
    return false;
  if (dict.empty())
    return false;

  // Read the property name and value from the dictionary (second argument).
  auto kv = *dict.begin();
  std::string property_name = kv.first;
  decltype(expected_property_value) property_value =
      kv.second.Get<decltype(expected_property_value)>();

  // Check if everything matches with expectations.
  return (arg->GetInterface() == dbus::kPropertiesInterface &&
          arg->GetMember() == dbus::kPropertiesChanged &&
          property_interface == expected_property_interface &&
          property_name == expected_property_name &&
          property_value == expected_property_value);
}

}  // namespace

class ExportedObjectManagerWrapperTest : public ::testing::Test {
 public:
  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    exported_object_manager_ =
        std::make_unique<brillo::dbus_utils::MockExportedObjectManager>(
            bus_, dbus::ObjectPath(kTestObjectManagerPath));
    object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));
  }

  void SetupPropertyMethodHandlers(
      brillo::dbus_utils::DBusInterface* prop_interface,
      brillo::dbus_utils::ExportedPropertySet* property_set) {}

  // The mocked dbus::ExportedObject::ExportMethod needs to call its callback.
  void StubExportMethod(
      const std::string& interface_name,
      const std::string& method_name,
      dbus::ExportedObject::MethodCallCallback method_call_callback,
      dbus::ExportedObject::OnExportedCallback on_exported_callback) {
    on_exported_callback.Run(interface_name, method_name, true /* success */);
  }

 protected:
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> object_proxy_;
  std::unique_ptr<brillo::dbus_utils::MockExportedObjectManager>
      exported_object_manager_;
};

TEST_F(ExportedObjectManagerWrapperTest, ExportedInterface) {
  dbus::ObjectPath object_path = dbus::ObjectPath(kTestObjectPath);

  scoped_refptr<dbus::MockExportedObject> exported_object =
      new dbus::MockExportedObject(bus_.get(), object_path);
  EXPECT_CALL(*bus_, GetExportedObject(object_path))
      .WillOnce(Return(exported_object.get()));
  EXPECT_CALL(*exported_object,
              ExportMethodAndBlock(dbus::kPropertiesInterface, _, _))
      .WillRepeatedly(Return(true));

  auto dbus_object = std::make_unique<brillo::dbus_utils::MockDBusObject>(
      exported_object_manager_.get(), bus_, object_path);
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path, dbus::kPropertiesInterface, _))
      .Times(1);
  dbus_object->RegisterAndBlock();

  auto interface = std::make_unique<ExportedInterface>(
      object_path, kTestInterfaceName1, dbus_object.get());
  // Instantiating ExportedInterface should add a DBusInterface to the
  // underlying DBusObject.
  EXPECT_TRUE(dbus_object->FindInterface(kTestInterfaceName1) != nullptr);

  interface->AddRawMethodHandler(
      kTestMethodName1,
      base::Bind(
          [](dbus::MethodCall*, dbus::ExportedObject::ResponseSender) {}));

  // ExportedInterface::ExportAsync should trigger the underlying
  // ExportedObjectManager to claim the interface of the specified object
  // and export the method handlers of that interface.
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path, kTestInterfaceName1, _))
      .Times(1);
  EXPECT_CALL(*exported_object,
              ExportMethod(kTestInterfaceName1, kTestMethodName1, _, _))
      .WillOnce(
          Invoke(this, &ExportedObjectManagerWrapperTest::StubExportMethod));
  interface->ExportAsync(base::Bind([](bool success) {}));

  // Register a property to the interface.
  PropertyFactory<int> property_factory;
  brillo::dbus_utils::ExportedPropertyBase* exported_property_base =
      interface->EnsureExportedPropertyRegistered(kTestPropertyName,
                                                  &property_factory);
  brillo::dbus_utils::ExportedProperty<int>* exported_property =
      static_cast<brillo::dbus_utils::ExportedProperty<int>*>(
          exported_property_base);
  int set_value = 7;
  // Check that signal is emitted when the registered property changes value.
  EXPECT_CALL(*exported_object,
              SendSignal(PropertySignalEq(kTestInterfaceName1,
                                          kTestPropertyName, set_value)))
      .Times(1);
  exported_property->SetValue(set_value);

  // Unexport() should remove the DBusInterface from the underlying DBusObject
  // and trigger the ExportedObjectManager to release the interface.
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, dbus::kPropertiesInterface))
      .Times(1);
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, kTestInterfaceName1))
      .Times(1);
  EXPECT_CALL(*exported_object, Unregister()).Times(1);
  EXPECT_CALL(*exported_object,
              UnexportMethodAndBlock(kTestInterfaceName1, kTestMethodName1))
      .WillOnce(Return(true));
  interface->Unexport();
  EXPECT_TRUE(dbus_object->FindInterface(kTestInterfaceName1) == nullptr);

  // Check that the property is unregistered.
  EXPECT_EQ(nullptr,
            interface->GetRegisteredExportedProperty(kTestPropertyName));
}

TEST_F(ExportedObjectManagerWrapperTest, SyncProperty) {
  dbus::ObjectPath object_path(kTestObjectPath);
  auto dbus_object = std::make_unique<brillo::dbus_utils::MockDBusObject>(
      exported_object_manager_.get(), bus_, object_path);

  auto interface = std::make_unique<ExportedInterface>(
      object_path, kTestInterfaceName1, dbus_object.get());
  // Instantiating ExportedInterface should add a DBusInterface to the
  // underlying DBusObject.
  EXPECT_TRUE(dbus_object->FindInterface(kTestInterfaceName1) != nullptr);

  // Prepare property for testing.
  PropertyFactory<int> property_factory;
  std::unique_ptr<dbus::PropertyBase> property_base =
      property_factory.CreateProperty();
  auto property_set = std::make_unique<dbus::PropertySet>(
      object_proxy_.get(), kTestInterfaceName1,
      base::Bind([](const std::string& property_name) {}));
  property_base->Init(property_set.get(), kTestPropertyName);
  property_base->set_valid(true);
  dbus::Property<int>* property =
      static_cast<dbus::Property<int>*>(property_base.get());
  int expected_value = 3;
  EXPECT_CALL(*object_proxy_, MockCallMethodAndBlock(_, _)).Times(1);
  property->SetAndBlock(expected_value);
  property->ReplaceValueWithSetValue();
  ASSERT_EQ(expected_value, property->value());
  interface->ExportAsync(base::Bind([](bool success) {}));

  interface->SyncPropertyToExportedProperty(
      kTestPropertyName, property_base.get(), &property_factory);

  // Check that the property is exported and the exported property has the
  // same value as the origin property.
  brillo::dbus_utils::ExportedPropertyBase* exported_property_base =
      interface->GetRegisteredExportedProperty(kTestPropertyName);
  ASSERT_NE(nullptr, exported_property_base);
  brillo::dbus_utils::ExportedProperty<int>* exported_property =
      static_cast<brillo::dbus_utils::ExportedProperty<int>*>(
          exported_property_base);
  EXPECT_EQ(expected_value, exported_property->value());

  // Syncing an invalidated property will remove the exported property.
  property_base->set_valid(false);
  interface->SyncPropertyToExportedProperty(
      kTestPropertyName, property_base.get(), &property_factory);
  // Check that the property is no longer exported.
  ASSERT_EQ(nullptr,
            interface->GetRegisteredExportedProperty(kTestPropertyName));
}

TEST_F(ExportedObjectManagerWrapperTest, ExportedObject) {
  dbus::ObjectPath object_path(kTestObjectPath);

  scoped_refptr<dbus::MockExportedObject> exported_object =
      new dbus::MockExportedObject(bus_.get(), object_path);
  EXPECT_CALL(*bus_, GetExportedObject(object_path))
      .WillOnce(Return(exported_object.get()));
  auto object = std::make_unique<ExportedObject>(
      exported_object_manager_.get(), bus_, object_path,
      base::Bind(&ExportedObjectManagerWrapperTest::SetupPropertyMethodHandlers,
                 base::Unretained(this)));

  // RegisterAsync should export org.freedesktop.DBus.Properties interface.
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path, dbus::kPropertiesInterface, _))
      .Times(1);
  object->RegisterAsync(base::Bind([](bool success) {}));

  // Add an exported interface.
  object->AddExportedInterface(kTestInterfaceName1);
  ExportedInterface* interface =
      object->GetExportedInterface(kTestInterfaceName1);
  // ExportedInterface::ExportAsync should trigger the underlying
  // ExportedObjectManager to claim the interface of the specified object.
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path, kTestInterfaceName1, _))
      .Times(1);
  interface->ExportAsync(base::Bind([](bool success) {}));

  // RemoveExportedInterface should unexport that interface.
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, kTestInterfaceName1))
      .Times(1);
  object->RemoveExportedInterface(kTestInterfaceName1);

  // Destructing ExportedObject should unexport the previously
  // exported interfaces and org.freedesktop.DBus.Properties interface.
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, dbus::kPropertiesInterface))
      .Times(1);
  EXPECT_CALL(*exported_object, Unregister()).Times(1);
  object.reset();
}

TEST_F(ExportedObjectManagerWrapperTest, ExportedObjectManagerWrapper) {
  dbus::ObjectPath object_path(kTestObjectPath);

  scoped_refptr<dbus::MockExportedObject> exported_object =
      new dbus::MockExportedObject(bus_.get(), object_path);

  brillo::dbus_utils::MockExportedObjectManager* exported_object_manager =
      exported_object_manager_.get();
  EXPECT_CALL(*exported_object_manager, RegisterAsync(_)).Times(1);
  ExportedObjectManagerWrapper exported_om_wrapper(
      bus_, std::move(exported_object_manager_));

  EXPECT_CALL(*bus_, GetExportedObject(object_path))
      .WillOnce(Return(exported_object.get()));

  exported_om_wrapper.SetPropertyHandlerSetupCallback(
      base::Bind(&ExportedObjectManagerWrapperTest::SetupPropertyMethodHandlers,
                 base::Unretained(this)));

  // AddExportedInterface to the not-yet-exported object will trigger that
  // object to be exported.
  EXPECT_CALL(*exported_object_manager,
              ClaimInterface(object_path, dbus::kPropertiesInterface, _))
      .Times(1);
  exported_om_wrapper.AddExportedInterface(object_path, kTestInterfaceName1);

  EXPECT_CALL(*exported_object_manager,
              ClaimInterface(object_path, kTestInterfaceName1, _))
      .Times(1);
  exported_om_wrapper.GetExportedInterface(object_path, kTestInterfaceName1)
      ->ExportAsync(base::Bind([](bool success) {}));

  EXPECT_CALL(*exported_object_manager,
              ClaimInterface(object_path, kTestInterfaceName2, _))
      .Times(1);
  exported_om_wrapper.AddExportedInterface(object_path, kTestInterfaceName2);
  exported_om_wrapper.GetExportedInterface(object_path, kTestInterfaceName2)
      ->ExportAsync(base::Bind([](bool success) {}));

  // RemoveExportedInterface on 1 of the 2 exported interfaces will only
  // unregister the removed interface without unregistering the object.
  EXPECT_CALL(*exported_object_manager,
              ReleaseInterface(object_path, kTestInterfaceName1))
      .Times(1);
  exported_om_wrapper.RemoveExportedInterface(object_path, kTestInterfaceName1);

  // RemoveExportedInterface on the last interface should unregister that
  // interface and also unregister the object.
  EXPECT_CALL(*exported_object_manager,
              ReleaseInterface(object_path, kTestInterfaceName2))
      .Times(1);
  EXPECT_CALL(*exported_object_manager,
              ReleaseInterface(object_path, dbus::kPropertiesInterface))
      .Times(1);
  EXPECT_CALL(*exported_object, Unregister()).Times(1);
  exported_om_wrapper.RemoveExportedInterface(object_path, kTestInterfaceName2);
  Mock::VerifyAndClearExpectations(exported_object_manager);
  Mock::VerifyAndClearExpectations(exported_object.get());
}

}  // namespace bluetooth
