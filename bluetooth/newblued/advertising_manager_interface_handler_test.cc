// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/advertising_manager_interface_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/memory/ref_counted.h>
#include <brillo/dbus/mock_exported_object_manager.h>
#include <brillo/errors/error.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/mock_libnewblue.h"

namespace bluetooth {

class AdvertisingManagerInterfaceHandlerTest : public ::testing::Test {
 public:
  AdvertisingManagerInterfaceHandlerTest()
      : mock_libnewblue_(std::make_unique<MockLibNewblue>()),
        bus_(new dbus::MockBus(dbus::Bus::Options())),
        exported_object_manager_wrapper_(
            bus_,
            std::make_unique<brillo::dbus_utils::MockExportedObjectManager>(
                bus_, dbus::ObjectPath("/"))) {
    advertising_manager_interface_handler_ =
        std::make_unique<AdvertisingManagerInterfaceHandler>(
            mock_libnewblue_.get(), bus_, &exported_object_manager_wrapper_);
  }

 protected:
  std::unique_ptr<MockLibNewblue> mock_libnewblue_;
  scoped_refptr<dbus::MockBus> bus_;
  ExportedObjectManagerWrapper exported_object_manager_wrapper_;
  std::unique_ptr<AdvertisingManagerInterfaceHandler>
      advertising_manager_interface_handler_;
};

TEST_F(AdvertisingManagerInterfaceHandlerTest,
       HandleRegisterAndUnregisterAdvertisement) {
  dbus::ObjectPath object_path("/test");
  scoped_refptr<dbus::MockObjectProxy> mock_object_proxy =
      new dbus::MockObjectProxy(bus_.get(), "", object_path);
  brillo::ErrorPtr error;
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get()), array(nullptr), dict(nullptr);
  writer.OpenArray("{sv}", &array);
  array.OpenDictEntry(&dict);
  dict.AppendString(bluetooth_advertisement::kTypeProperty);
  dict.AppendVariantOfString(bluetooth_advertisement::kTypeBroadcast);
  array.CloseContainer(&dict);
  writer.CloseContainer(&array);
  EXPECT_CALL(*bus_, GetObjectProxy("", object_path))
      .WillOnce(testing::Return(mock_object_proxy.get()));
  EXPECT_CALL(*mock_object_proxy,
              MockCallMethodAndBlock(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(response.release()));
  EXPECT_CALL(*mock_libnewblue_, HciAdvSetEnable(::testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(
      advertising_manager_interface_handler_->HandleRegisterAdvertisement(
          &error, dbus::Response::CreateEmpty().get(), object_path,
          brillo::VariantDictionary()));
  EXPECT_TRUE(
      advertising_manager_interface_handler_->HandleUnregisterAdvertisement(
          &error, dbus::Response::CreateEmpty().get(), object_path));
  EXPECT_FALSE(
      advertising_manager_interface_handler_->HandleUnregisterAdvertisement(
          &error, dbus::Response::CreateEmpty().get(), object_path));
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, AddType) {
  std::vector<uint8_t> data;
  brillo::ErrorPtr error;
  EXPECT_FALSE(advertising_manager_interface_handler_->AddType("random trash",
                                                               &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(advertising_manager_interface_handler_->AddType(
      bluetooth_advertisement::kTypeBroadcast, &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(advertising_manager_interface_handler_->AddType(
      bluetooth_advertisement::kTypePeripheral, &data, &error));
  EXPECT_EQ(std::vector<uint8_t>({/* length = */ 2, HCI_EIR_TYPE_FLAGS,
                                  /*kGeneralDiscoverable */ 2}),
            data);
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, AddIncludeTxPower) {
  std::vector<uint8_t> data;
  brillo::ErrorPtr error;
  EXPECT_TRUE(advertising_manager_interface_handler_->AddIncludeTxPower(
      false, &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(advertising_manager_interface_handler_->AddIncludeTxPower(
      true, &data, &error));
  EXPECT_EQ(std::vector<uint8_t>({/* length = */ 2, HCI_EIR_TX_POWER_LEVEL,
                                  HCI_ADV_TX_PWR_LVL_DONT_CARE}),
            data);
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, AddServiceUuid) {
  std::vector<uint8_t> data;
  brillo::ErrorPtr error;
  EXPECT_FALSE(advertising_manager_interface_handler_->AddServiceUuid(
      {"+-*/"}, &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(advertising_manager_interface_handler_->AddServiceUuid(
      {"1234"}, &data, &error));
  EXPECT_EQ(
      std::vector<uint8_t>({/* length = */ 17, HCI_EIR_TYPE_COMPL_LIST_UUID128,
                            0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                            0x00, 0x10, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00}),
      data);
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, AddSolicitUuid) {
  std::vector<uint8_t> data;
  brillo::ErrorPtr error;
  EXPECT_FALSE(advertising_manager_interface_handler_->AddSolicitUuid(
      {"+-*/"}, &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(advertising_manager_interface_handler_->AddSolicitUuid(
      {"1234"}, &data, &error));
  EXPECT_EQ(
      std::vector<uint8_t>({/* length = */ 17, HCI_EIR_SVC_SOLICITS_UUID128,
                            0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                            0x00, 0x10, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00}),
      data);
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, AddServiceData) {
  std::vector<uint8_t> data;
  brillo::ErrorPtr error;
  EXPECT_FALSE(advertising_manager_interface_handler_->AddServiceData(
      {{"+-*/", std::vector<uint8_t>({0x07})}}, &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_FALSE(advertising_manager_interface_handler_->AddServiceData(
      {{"1234", std::vector<uint8_t>(UINT8_MAX, 0x07)}}, &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(advertising_manager_interface_handler_->AddServiceData(
      {{"1234", std::vector<uint8_t>({0x07, 0x06})}}, &data, &error));
  EXPECT_EQ(std::vector<uint8_t>({/* length = */ 19,
                                  HCI_EIR_SVC_DATA_UUID128,
                                  0xfb,
                                  0x34,
                                  0x9b,
                                  0x5f,
                                  0x80,
                                  0x00,
                                  0x00,
                                  0x80,
                                  0x00,
                                  0x10,
                                  0x00,
                                  0x00,
                                  0x34,
                                  0x12,
                                  0x00,
                                  0x00,
                                  0x06,
                                  0x07}),
            data);
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, AddManufacturerData) {
  std::vector<uint8_t> data;
  brillo::ErrorPtr error;
  EXPECT_FALSE(advertising_manager_interface_handler_->AddManufacturerData(
      {{0x0304, std::vector<uint8_t>(UINT8_MAX, 0x07)}}, &data, &error));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(advertising_manager_interface_handler_->AddManufacturerData(
      {{0x0304, std::vector<uint8_t>({0x07, 0x05})}}, &data, &error));
  EXPECT_EQ(std::vector<uint8_t>(
                {/* length = */ 5, HCI_EIR_MANUF_DATA, 0x04, 0x03, 0x05, 0x07}),
            data);
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, ConfigureData) {
  hci_adv_set_t handle = hciAdvSetAllocate();
  brillo::ErrorPtr error;
  EXPECT_FALSE(
      advertising_manager_interface_handler_->ConfigureData(0, {}, &error));
  EXPECT_TRUE(advertising_manager_interface_handler_->ConfigureData(handle, {},
                                                                    &error));
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, SetParams) {
  hci_adv_set_t handle = hciAdvSetAllocate();
  brillo::ErrorPtr error;
  EXPECT_FALSE(advertising_manager_interface_handler_->SetParams(0, &error));
  EXPECT_TRUE(
      advertising_manager_interface_handler_->SetParams(handle, &error));
}

TEST_F(AdvertisingManagerInterfaceHandlerTest, Enable) {
  hci_adv_set_t handle = hciAdvSetAllocate();
  brillo::ErrorPtr error;
  EXPECT_CALL(*mock_libnewblue_, HciAdvSetEnable(handle))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(advertising_manager_interface_handler_->Enable(handle, &error));
  EXPECT_CALL(*mock_libnewblue_, HciAdvSetEnable(handle))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(advertising_manager_interface_handler_->Enable(handle, &error));
}

}  // namespace bluetooth
