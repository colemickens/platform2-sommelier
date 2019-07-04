// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_ADVERTISING_MANAGER_INTERFACE_HANDLER_H_
#define BLUETOOTH_NEWBLUED_ADVERTISING_MANAGER_INTERFACE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <brillo/errors/error.h>
#include <brillo/variant_dictionary.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <gtest/gtest_prod.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/libnewblue.h"

namespace bluetooth {

class AdvertisementProperties;

// TODO(crbug/898601): Should we support SetAdvertisingIntervals and
//                     ResetAdvertising, which allows clients to change the
//                     global advertisement state?
class AdvertisingManagerInterfaceHandler {
 public:
  AdvertisingManagerInterfaceHandler(
      LibNewblue* libnewblue,
      scoped_refptr<dbus::Bus> bus,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper);
  void Init();
  bool HandleRegisterAdvertisement(brillo::ErrorPtr* error,
                                   dbus::Message* message,
                                   dbus::ObjectPath object_path,
                                   brillo::VariantDictionary options);
  bool HandleUnregisterAdvertisement(brillo::ErrorPtr* error,
                                     dbus::Message* message,
                                     dbus::ObjectPath object_path);

 private:
  bool AddType(const std::string& type,
               std::vector<uint8_t>* data,
               brillo::ErrorPtr* error);
  bool AddIncludeTxPower(bool include_tx_power,
                         std::vector<uint8_t>* data,
                         brillo::ErrorPtr* error);
  bool AddServiceUuid(const std::vector<std::string>& service_uuids,
                      std::vector<uint8_t>* data,
                      brillo::ErrorPtr* error);
  bool AddSolicitUuid(const std::vector<std::string>& solicit_uuids,
                      std::vector<uint8_t>* data,
                      brillo::ErrorPtr* error);
  bool AddServiceData(
      const std::map<std::string, std::vector<uint8_t>>& service_data,
      std::vector<uint8_t>* data,
      brillo::ErrorPtr* error);
  bool AddManufacturerData(
      const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data,
      std::vector<uint8_t>* data,
      brillo::ErrorPtr* error);
  bool ConstructData(const AdvertisementProperties& properties,
                     std::vector<uint8_t>* data,
                     brillo::ErrorPtr* error);
  bool ConfigureData(hci_adv_set_t handle,
                     const std::vector<uint8_t>& data,
                     brillo::ErrorPtr* error);
  bool SetParams(hci_adv_set_t handle, brillo::ErrorPtr* error);
  bool Enable(hci_adv_set_t handle, brillo::ErrorPtr* error);

  LibNewblue* libnewblue_;  // Not owned.
  scoped_refptr<dbus::Bus> bus_;
  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;
  std::map<dbus::ObjectPath, hci_adv_set_t> handles_;

  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, AddType);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, AddIncludeTxPower);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, AddServiceUuid);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, AddSolicitUuid);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, AddServiceData);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, AddManufacturerData);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, ConfigureData);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, SetParams);
  FRIEND_TEST(AdvertisingManagerInterfaceHandlerTest, Enable);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_ADVERTISING_MANAGER_INTERFACE_HANDLER_H_
