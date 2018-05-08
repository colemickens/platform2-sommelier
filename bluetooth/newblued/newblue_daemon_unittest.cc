// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/message_loop/message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_manager.h>
#include <gtest/gtest.h>

#include "bluetooth/newblued/newblue_daemon.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace bluetooth {

namespace {

constexpr char kAdapterObjectPath[] = "/org/bluez/hci0";

}  // namespace

class NewblueDaemonTest : public ::testing::Test {
 public:
  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    newblued_ = std::make_unique<NewblueDaemon>();
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<dbus::MockBus> bus_;
  std::unique_ptr<NewblueDaemon> newblued_;
};

TEST_F(NewblueDaemonTest, Default) {
  dbus::ObjectPath root_path(
      newblue_object_manager::kNewblueObjectManagerServicePath);
  scoped_refptr<dbus::MockExportedObject> exported_root_object =
      new dbus::MockExportedObject(bus_.get(), root_path);
  EXPECT_CALL(*bus_, GetExportedObject(root_path))
      .WillOnce(Return(exported_root_object.get()));

  dbus::ObjectPath adapter_object_path(kAdapterObjectPath);
  scoped_refptr<dbus::MockExportedObject> exported_adapter_object =
      new dbus::MockExportedObject(bus_.get(), adapter_object_path);
  EXPECT_CALL(*bus_, GetExportedObject(adapter_object_path))
      .WillOnce(Return(exported_adapter_object.get()));

  EXPECT_CALL(*bus_,
              RequestOwnershipAndBlock(
                  newblue_object_manager::kNewblueObjectManagerServiceName,
                  dbus::Bus::REQUIRE_PRIMARY))
      .WillOnce(Return(true));

  // These are expected to be exported on the root object.
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

  // Some properties are expected to be exported on the adapter object.
  EXPECT_CALL(
      *exported_adapter_object,
      ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesGetAll, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      *exported_adapter_object,
      ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesGet, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      *exported_adapter_object,
      ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesSet, _, _))
      .Times(AnyNumber());

  newblued_->Init(bus_);

  EXPECT_CALL(*exported_adapter_object, Unregister()).Times(1);
  EXPECT_CALL(*exported_root_object, Unregister()).Times(1);
  // Shutdown now to make sure ExportedObjectManagerWrapper is destructed first
  // before the mocked objects.
  newblued_->Shutdown();
}

}  // namespace bluetooth
