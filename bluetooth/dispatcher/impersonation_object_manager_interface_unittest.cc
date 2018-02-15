// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/impersonation_object_manager_interface.h"

#include <memory>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <brillo/bind_lambda.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_manager.h>
#include <dbus/mock_object_proxy.h>
#include <brillo/dbus/exported_property_set.h>
#include <brillo/dbus/mock_dbus_object.h>
#include <brillo/dbus/mock_exported_object_manager.h>
#include <dbus/property.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;

namespace bluetooth {

namespace {

constexpr char kTestInterfaceName1[] = "org.example.Interface1";
constexpr char kTestInterfaceName2[] = "org.example.Interface2";
constexpr char kTestObjectPath1[] = "/org/example/Object1";
constexpr char kTestObjectPath2[] = "/org/example/Object2";
constexpr char kTestObjectManagerPath[] = "/";
constexpr char kTestServiceName[] = "org.example.Default";

constexpr char kStringPropertyName[] = "SomeString";
constexpr char kIntPropertyName[] = "SomeInt";
constexpr char kBoolPropertyName[] = "SomeBool";

}  // namespace

class TestInterfaceHandler : public InterfaceHandler {
 public:
  TestInterfaceHandler() {
    property_factory_map_[kStringPropertyName] =
        std::make_unique<PropertyFactory<std::string>>();
    property_factory_map_[kIntPropertyName] =
        std::make_unique<PropertyFactory<int>>();
    property_factory_map_[kBoolPropertyName] =
        std::make_unique<PropertyFactory<bool>>();
  }
  PropertyFactoryMap* GetPropertyFactoryMap() override {
    return &property_factory_map_;
  }

 private:
  InterfaceHandler::PropertyFactoryMap property_factory_map_;
};

class ImpersonationObjectManagerInterfaceTest : public ::testing::Test {
 public:
  void SetUp() {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    EXPECT_CALL(*bus_, GetDBusTaskRunner())
        .Times(1)
        .WillOnce(Return(message_loop_.task_runner().get()));
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, Connect()).WillRepeatedly(Return(false));

    dbus::ObjectPath object_manager_path(kTestObjectManagerPath);
    object_manager_object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), kTestServiceName, object_manager_path);
    EXPECT_CALL(*bus_, GetObjectProxy(kTestServiceName, object_manager_path))
        .WillOnce(Return(object_manager_object_proxy_.get()));
    object_manager_ = new dbus::MockObjectManager(bus_.get(), kTestServiceName,
                                                  object_manager_path);
    // Forces MessageLoop to run pending tasks as effect of instantiating
    // MockObjectManager. Needed to avoid memory leaks because pending tasks
    // are unowned pointers that will only self destruct after being run.
    message_loop_.RunUntilIdle();
    auto exported_object_manager =
        std::make_unique<brillo::dbus_utils::MockExportedObjectManager>(
            bus_, object_manager_path);
    exported_object_manager_ = exported_object_manager.get();
    EXPECT_CALL(*exported_object_manager_, RegisterAsync(_)).Times(1);
    exported_object_manager_wrapper_ =
        std::make_unique<ExportedObjectManagerWrapper>(
            bus_, std::move(exported_object_manager));
    object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath1));
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> object_manager_object_proxy_;
  scoped_refptr<dbus::MockObjectProxy> object_proxy_;
  scoped_refptr<dbus::MockObjectManager> object_manager_;
  std::unique_ptr<ExportedObjectManagerWrapper>
      exported_object_manager_wrapper_;
  brillo::dbus_utils::MockExportedObjectManager* exported_object_manager_;
};

TEST_F(ImpersonationObjectManagerInterfaceTest, SingleInterface) {
  dbus::ObjectPath object_path1(kTestObjectPath1);
  dbus::ObjectPath object_path2(kTestObjectPath2);

  std::map<std::string, std::unique_ptr<ExportedObject>> exported_objects;
  auto impersonation_om_interface =
      std::make_unique<ImpersonationObjectManagerInterface>(
          bus_, object_manager_.get(), exported_object_manager_wrapper_.get(),
          std::make_unique<TestInterfaceHandler>(), kTestInterfaceName1);

  scoped_refptr<dbus::MockExportedObject> exported_object1 =
      new dbus::MockExportedObject(bus_.get(), object_path1);
  EXPECT_CALL(*bus_, GetExportedObject(object_path1))
      .WillOnce(Return(exported_object1.get()));
  scoped_refptr<dbus::MockExportedObject> exported_object2 =
      new dbus::MockExportedObject(bus_.get(), object_path2);
  EXPECT_CALL(*bus_, GetExportedObject(object_path2))
      .WillOnce(Return(exported_object2.get()));

  // D-Bus properties methods should be exported.
  EXPECT_CALL(*exported_object1, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesGet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object1, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesSet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object1, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesGetAll, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object1, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesChanged, _, _))
      .Times(1);
  // CreateProperties called for an object.
  std::unique_ptr<dbus::PropertySet> dbus_property_set1(
      impersonation_om_interface->CreateProperties(
          kTestServiceName, object_proxy_.get(), object_path1,
          kTestInterfaceName1));
  PropertySet* property_set1 =
      static_cast<PropertySet*>(dbus_property_set1.get());
  // The properties should all be registered.
  ASSERT_NE(nullptr, property_set1->GetProperty(kStringPropertyName));
  ASSERT_NE(nullptr, property_set1->GetProperty(kIntPropertyName));
  ASSERT_NE(nullptr, property_set1->GetProperty(kBoolPropertyName));

  // D-Bus properties methods should be exported.
  EXPECT_CALL(*exported_object2, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesGet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object2, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesSet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object2, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesGetAll, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object2, ExportMethod(dbus::kPropertiesInterface,
                                              dbus::kPropertiesChanged, _, _))
      .Times(1);
  // CreateProperties called for another object.
  std::unique_ptr<dbus::PropertySet> dbus_property_set2(
      impersonation_om_interface->CreateProperties(
          kTestServiceName, object_proxy_.get(), object_path2,
          kTestInterfaceName1));
  PropertySet* property_set2 =
      static_cast<PropertySet*>(dbus_property_set2.get());

  // The properties should all be registered.
  ASSERT_NE(nullptr, property_set2->GetProperty(kStringPropertyName));
  ASSERT_NE(nullptr, property_set2->GetProperty(kIntPropertyName));
  ASSERT_NE(nullptr, property_set2->GetProperty(kBoolPropertyName));

  // ObjectAdded events
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path1, kTestInterfaceName1, _))
      .Times(1);
  impersonation_om_interface->ObjectAdded(kTestServiceName, object_path1,
                                          kTestInterfaceName1);
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path2, kTestInterfaceName1, _))
      .Times(1);
  impersonation_om_interface->ObjectAdded(kTestServiceName, object_path2,
                                          kTestInterfaceName1);

  // ObjectRemoved events
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path1, kTestInterfaceName1))
      .Times(1);
  EXPECT_CALL(*exported_object1, Unregister()).Times(1);
  impersonation_om_interface->ObjectRemoved(kTestServiceName, object_path1,
                                            kTestInterfaceName1);
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path2, kTestInterfaceName1))
      .Times(1);
  EXPECT_CALL(*exported_object2, Unregister()).Times(1);
  impersonation_om_interface->ObjectRemoved(kTestServiceName, object_path2,
                                            kTestInterfaceName1);
}

TEST_F(ImpersonationObjectManagerInterfaceTest, MultipleInterfaces) {
  dbus::ObjectPath object_path(kTestObjectPath1);

  scoped_refptr<dbus::MockExportedObject> exported_object =
      new dbus::MockExportedObject(bus_.get(), object_path);
  EXPECT_CALL(*bus_, GetExportedObject(object_path))
      .WillOnce(Return(exported_object.get()));

  std::map<std::string, std::unique_ptr<ExportedObject>> exported_objects;
  auto impersonation_om_interface1 =
      std::make_unique<ImpersonationObjectManagerInterface>(
          bus_, object_manager_.get(), exported_object_manager_wrapper_.get(),
          std::make_unique<TestInterfaceHandler>(), kTestInterfaceName1);
  auto impersonation_om_interface2 =
      std::make_unique<ImpersonationObjectManagerInterface>(
          bus_, object_manager_.get(), exported_object_manager_wrapper_.get(),
          std::make_unique<TestInterfaceHandler>(), kTestInterfaceName2);

  // D-Bus properties methods should be exported.
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesGet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesSet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesGetAll, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesChanged, _, _))
      .Times(1);
  // CreateProperties called for an object.
  std::unique_ptr<dbus::PropertySet> dbus_property_set1(
      impersonation_om_interface1->CreateProperties(
          kTestServiceName, object_proxy_.get(), object_path,
          kTestInterfaceName1));
  PropertySet* property_set1 =
      static_cast<PropertySet*>(dbus_property_set1.get());

  std::unique_ptr<dbus::PropertySet> dbus_property_set2(
      impersonation_om_interface2->CreateProperties(
          kTestServiceName, object_proxy_.get(), object_path,
          kTestInterfaceName2));
  PropertySet* property_set2 =
      static_cast<PropertySet*>(dbus_property_set2.get());

  // The properties should all be registered.
  ASSERT_NE(nullptr, property_set1->GetProperty(kStringPropertyName));
  ASSERT_NE(nullptr, property_set1->GetProperty(kIntPropertyName));
  ASSERT_NE(nullptr, property_set1->GetProperty(kBoolPropertyName));
  ASSERT_NE(nullptr, property_set2->GetProperty(kStringPropertyName));
  ASSERT_NE(nullptr, property_set2->GetProperty(kIntPropertyName));
  ASSERT_NE(nullptr, property_set2->GetProperty(kBoolPropertyName));

  // ObjectAdded events
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path, kTestInterfaceName1, _))
      .Times(1);
  impersonation_om_interface1->ObjectAdded(kTestServiceName, object_path,
                                           kTestInterfaceName1);
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path, kTestInterfaceName2, _))
      .Times(1);
  impersonation_om_interface2->ObjectAdded(kTestServiceName, object_path,
                                           kTestInterfaceName2);

  // ObjectRemoved events
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, kTestInterfaceName1))
      .Times(1);
  // Exported object shouldn't be unregistered until the last interface is
  // removed.
  EXPECT_CALL(*exported_object, Unregister()).Times(0);
  impersonation_om_interface1->ObjectRemoved(kTestServiceName, object_path,
                                             kTestInterfaceName1);

  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, kTestInterfaceName2))
      .Times(1);
  // Now that the last interface has been removed, exported object should be
  // unregistered.
  EXPECT_CALL(*exported_object, Unregister()).Times(1);
  impersonation_om_interface2->ObjectRemoved(kTestServiceName, object_path,
                                             kTestInterfaceName2);

  // Makes sure that the Unregister actually happens on ObjectRemoved above and
  // not due to its automatic deletion when this test case finishes.
  Mock::VerifyAndClearExpectations(exported_object.get());
}

TEST_F(ImpersonationObjectManagerInterfaceTest, UnexpectedEvents) {
  dbus::ObjectPath object_path(kTestObjectPath1);

  scoped_refptr<dbus::MockExportedObject> exported_object =
      new dbus::MockExportedObject(bus_.get(), object_path);
  EXPECT_CALL(*bus_, GetExportedObject(object_path))
      .WillOnce(Return(exported_object.get()));

  std::map<std::string, std::unique_ptr<ExportedObject>> exported_objects;
  auto impersonation_om_interface =
      std::make_unique<ImpersonationObjectManagerInterface>(
          bus_, object_manager_.get(), exported_object_manager_wrapper_.get(),
          std::make_unique<TestInterfaceHandler>(), kTestInterfaceName1);

  // ObjectAdded event happens before CreateProperties. This shouldn't happen.
  // Makes sure we only ignore the event and don't crash if this happens.
  EXPECT_CALL(*exported_object_manager_,
              ClaimInterface(object_path, kTestInterfaceName1, _))
      .Times(0);
  impersonation_om_interface->ObjectAdded(kTestServiceName, object_path,
                                          kTestInterfaceName1);

  // ObjectRemoved event happens before CreateProperties. This shouldn't happen.
  // Makes sure we only ignore the event and don't crash if this happens.
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, kTestInterfaceName1))
      .Times(0);
  EXPECT_CALL(*exported_object, Unregister()).Times(0);
  impersonation_om_interface->ObjectRemoved(kTestServiceName, object_path,
                                            kTestInterfaceName1);

  // D-Bus properties methods should be exported.
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesGet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesSet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesGetAll, _, _))
      .Times(1);
  EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                             dbus::kPropertiesChanged, _, _))
      .Times(1);
  // CreateProperties called for an object.
  std::unique_ptr<dbus::PropertySet> dbus_property_set(
      impersonation_om_interface->CreateProperties(
          kTestServiceName, object_proxy_.get(), object_path,
          kTestInterfaceName1));
  PropertySet* property_set =
      static_cast<PropertySet*>(dbus_property_set.get());

  // The properties should all be registered.
  ASSERT_NE(nullptr, property_set->GetProperty(kStringPropertyName));
  ASSERT_NE(nullptr, property_set->GetProperty(kIntPropertyName));
  ASSERT_NE(nullptr, property_set->GetProperty(kBoolPropertyName));

  // ObjectRemoved event happens before ObjectAdded. This shouldn't happen.
  // Makes sure we still handle this gracefully.
  EXPECT_CALL(*exported_object_manager_,
              ReleaseInterface(object_path, kTestInterfaceName1))
      .Times(0);
  EXPECT_CALL(*exported_object, Unregister()).Times(1);
  impersonation_om_interface->ObjectRemoved(kTestServiceName, object_path,
                                            kTestInterfaceName1);

  // Makes sure that the Unregister actually happens on ObjectRemoved above and
  // not due to its automatic deletion when this test case finishes.
  Mock::VerifyAndClearExpectations(exported_object.get());
}

}  // namespace bluetooth
