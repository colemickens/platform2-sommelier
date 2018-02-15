// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include <base/message_loop/message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_manager.h>
#include <dbus/mock_object_proxy.h>
#include <gtest/gtest.h>

#include "bluetooth/dispatcher/dispatcher.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace bluetooth {

class DispatcherTest : public ::testing::Test {
 public:
  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, GetDBusTaskRunner())
        .Times(1)
        .WillOnce(Return(message_loop_.task_runner().get()));
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, Connect()).WillRepeatedly(Return(false));

    dbus::ObjectPath object_manager_path(
        bluez_object_manager::kBluezObjectManagerServicePath);
    object_manager_object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), bluez_object_manager::kBluezObjectManagerServiceName,
        object_manager_path);
    EXPECT_CALL(*bus_, GetObjectProxy(
                           bluez_object_manager::kBluezObjectManagerServiceName,
                           object_manager_path))
        .WillOnce(Return(object_manager_object_proxy_.get()));
    bluez_object_manager_ = new dbus::MockObjectManager(
        bus_.get(), bluez_object_manager::kBluezObjectManagerServiceName,
        object_manager_path);
    // Forces MessageLoop to run pending tasks as effect of instantiating
    // MockObjectManager. Needed to avoid memory leaks because pending tasks
    // are unowned pointers that will only self destruct after being run.
    message_loop_.RunUntilIdle();
    dispatcher_ = std::make_unique<Dispatcher>(bus_);
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> object_manager_object_proxy_;
  scoped_refptr<dbus::MockObjectManager> bluez_object_manager_;
  std::unique_ptr<Dispatcher> dispatcher_;
};

TEST_F(DispatcherTest, Default) {
  dbus::ObjectPath root_path(
      bluetooth_object_manager::kBluetoothObjectManagerServicePath);

  scoped_refptr<dbus::MockExportedObject> exported_root_object =
      new dbus::MockExportedObject(bus_.get(), root_path);
  EXPECT_CALL(*bus_, GetExportedObject(root_path))
      .WillOnce(Return(exported_root_object.get()));

  EXPECT_CALL(
      *bus_,
      RequestOwnershipAndBlock(
          bluetooth_object_manager::kBluetoothObjectManagerServiceName, _))
      .WillOnce(Return(true));

  EXPECT_CALL(*bus_, GetObjectManager(
                         bluez_object_manager::kBluezObjectManagerServiceName,
                         root_path))
      .WillOnce(Return(bluez_object_manager_.get()));

  // org.freedesktop.DBus.ObjectManager interface methods should be exported.
  EXPECT_CALL(*exported_root_object,
              ExportMethod(dbus::kObjectManagerInterface,
                           dbus::kObjectManagerGetManagedObjects, _, _))
      .Times(1);
  EXPECT_CALL(*exported_root_object,
              ExportMethod(dbus::kObjectManagerInterface,
                           dbus::kObjectManagerInterfacesAdded, _, _))
      .Times(1);
  EXPECT_CALL(*exported_root_object,
              ExportMethod(dbus::kObjectManagerInterface,
                           dbus::kObjectManagerInterfacesRemoved, _, _))
      .Times(1);

  // org.freedesktop.DBus.Properties interface methods should be exported.
  EXPECT_CALL(*exported_root_object, ExportMethod(dbus::kPropertiesInterface,
                                                  dbus::kPropertiesGet, _, _))
      .Times(1);
  EXPECT_CALL(*exported_root_object, ExportMethod(dbus::kPropertiesInterface,
                                                  dbus::kPropertiesSet, _, _))
      .Times(1);
  EXPECT_CALL(
      *exported_root_object,
      ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesGetAll, _, _))
      .Times(1);

  std::vector<std::string> bluez_interfaces = {
      bluetooth_adapter::kBluetoothAdapterInterface,
      bluetooth_device::kBluetoothDeviceInterface,
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
      bluetooth_input::kBluetoothInputInterface,
      bluetooth_media::kBluetoothMediaInterface,
      bluetooth_gatt_service::kBluetoothGattServiceInterface,
      bluetooth_advertising_manager::kBluetoothAdvertisingManagerInterface,
      bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
      bluetooth_media_transport::kBluetoothMediaTransportInterface,
  };

  // Should listen to BlueZ interfaces.
  for (const std::string& interface_name : bluez_interfaces)
    EXPECT_CALL(*bluez_object_manager_, RegisterInterface(interface_name, _));
  dispatcher_->Init();

  // Frees up all resources.
  EXPECT_CALL(*exported_root_object, Unregister()).Times(1);
  for (const std::string& interface_name : bluez_interfaces)
    EXPECT_CALL(*bluez_object_manager_, UnregisterInterface(interface_name));
  dispatcher_->Shutdown();
}

}  // namespace bluetooth
