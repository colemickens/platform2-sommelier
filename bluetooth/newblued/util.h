// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_UTIL_H_
#define BLUETOOTH_NEWBLUED_UTIL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <newblue/bt.h>
#include <newblue/gatt.h>
#include <newblue/sg.h>
#include <newblue/uuid.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/device_interface_handler.h"
#include "bluetooth/newblued/gatt_attributes.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/property.h"
#include "bluetooth/newblued/uuid.h"

namespace bluetooth {

constexpr int16_t kInvalidServiceHandle = -1;
constexpr int32_t kInvalidCharacteristicHandle = -1;
constexpr int16_t kInvalidDescriptorHandle = -1;

// TODO(mcchou): Move GATT assigned number and masks to a separate file.
constexpr uint32_t kAppearanceMask = 0xffc0;

////////////////////////////////////////////////////////////////////////////////
// Miscellaneous utility functions
////////////////////////////////////////////////////////////////////////////////

// Turns the content of |buf| into a uint16_t in host order. This should be used
// when reading the little-endian data from Bluetooth packet.
uint16_t GetNumFromLE16(const uint8_t* buf);

// Turns the content of |buf| into a uint32_t in host order. This should be used
// when reading the little-endian data from Bluetooth packet.
uint32_t GetNumFromLE24(const uint8_t* buf);

// Reverses the content of |buf| and returns bytes in big-endian order. This
// should be used when reading the little-endian data from Bluetooth packet.
std::vector<uint8_t> GetBytesFromLE(const uint8_t* buf, size_t buf_len);

// Retrieves a unique identifier which can be used for tracking the clients and
// the data associated with them.
UniqueId GetNextId();

template <typename T>
bool GetVariantValue(const brillo::VariantDictionary& dictionary,
                     const std::string& key,
                     T* output) {
  brillo::VariantDictionary::const_iterator it = dictionary.find(key);
  if (it == dictionary.end())
    return false;
  *output = it->second.TryGet<T>();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Parsing Discovered Device Information
////////////////////////////////////////////////////////////////////////////////

// Convert device appearance into icon
std::string ConvertAppearanceToIcon(uint16_t appearance);

// Filters out non-ASCII characters from a string by replacing them with spaces.
std::string ConvertToAsciiString(std::string name);

std::map<uint16_t, std::vector<uint8_t>> ParseDataIntoManufacturer(
    uint16_t manufacturer_id, std::vector<uint8_t> manufacturer_data);

// Updates the service UUIDs based on data of EirTypes including
// UUID16_INCOMPLETE, UUID16_COMPLETE, UUID32_INCOMPLETE, UUID32_COMPLETE,
// UUID128_INCOMPLETE and UUID128_COMPLETE.
void ParseDataIntoUuids(std::set<Uuid>* service_uuids,
                        const uint8_t uuid_size,
                        const uint8_t* data,
                        const uint8_t data_len);

// Updates the service data based on data of EirTypes including SVC_DATA16,
// SVC_DATA32 and SVC_DATA128.
void ParseDataIntoServiceData(
    std::map<Uuid, std::vector<uint8_t>>* service_data,
    const uint8_t uuid_size,
    const uint8_t* data,
    const uint8_t data_len);

////////////////////////////////////////////////////////////////////////////////
// D-Bus object path helpers and export helpers.
////////////////////////////////////////////////////////////////////////////////

bool TrimAdapterFromObjectPath(std::string* path);
std::string TrimDeviceFromObjectPath(std::string* device);
int32_t TrimServiceFromObjectPath(std::string* service);
int32_t TrimCharacteristicFromObjectPath(std::string* characteristic);
int32_t TrimDescriptorFromObjectPath(std::string* descriptor);

// Converts device object path to device address, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05 will be 00:01:02:03:04:05.
// Return a valid address if |path| is valid; empty string otherwise.
std::string ConvertDeviceObjectPathToAddress(const std::string& path);

// Converts device object path from device address, e.g.
// 00:01:02:03:04:05 will be /org/bluez/hci0/dev_00_01_02_03_04_05
dbus::ObjectPath ConvertDeviceAddressToObjectPath(const std::string& address);

// Converts GATT service object path to service handle, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF will return true with
// device address 00:01:02:03:04:05 and service handle 0X01FF.
bool ConvertServiceObjectPathToHandle(std::string* address,
                                      uint16_t* handle,
                                      const std::string& path);

// Converts service handle to service object path, e.g. with device address
// 00:01:02:03:04:05 and service handle 0X01FF, this will return
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF.
dbus::ObjectPath ConvertServiceHandleToObjectPath(const std::string& address,
                                                  uint16_t handle);

// Converts GATT characteristic object path to service handle, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123 will return true
// with device address 00:01:02:03:04:05, service handle 0X01FF and
// characteristic handle 0x0123.
bool ConvertCharacteristicObjectPathToHandles(std::string* address,
                                              uint16_t* service_handle,
                                              uint16_t* char_handle,
                                              const std::string& path);

// Converts characteristic handle to characteristic object path, e.g. with
// device address 00:01:02:03:04:05, service handle 0X01FF and characteristic
// handle 0x0123, this will return
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123.
dbus::ObjectPath ConvertCharacteristicHandleToObjectPath(
    const std::string& address, uint16_t service_handle, uint16_t char_handle);

// Converts GATT descriptor object path to service handle, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123/descriptor0012
// will return true with device address 00:01:02:03:04:05, service handle
// 0X01FF, characteristic handle 0x0123 and descriptor handle 0x0012.
bool ConvertDescriptorObjectPathToHandles(std::string* address,
                                          uint16_t* service_handle,
                                          uint16_t* char_handle,
                                          uint16_t* desc_handle,
                                          const std::string& path);

// Converts descriptor handle to descriptor object path, e.g. with device
// address 00:01:02:03:04:05, service handle 0X01FF, characteristic handle
// 0x0123 and descriptor handle 0x0012, this will return
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123/descriptor0012.
dbus::ObjectPath ConvertDescriptorHandleToObjectPath(const std::string& address,
                                                     uint16_t service_handle,
                                                     uint16_t char_handle,
                                                     uint16_t desc_handle);

// Exposes or updates the device object's property depends on the whether it
// was exposed before or should be forced updated.
template <typename T>
void ExportDBusProperty(ExportedInterface* interface,
                        const std::string& property_name,
                        const Property<T>& property,
                        bool force_export) {
  CHECK(interface);
  CHECK(!property_name.empty());

  if (force_export || property.updated()) {
    interface->EnsureExportedPropertyRegistered<T>(property_name)
        ->SetValue(property.value());
  }
}

// Exposes or updates the device object's property depends on the whether it
// was exposed before or should be forced updated. Takes a converter function
// which converts the value of a property into the value for exposing.
template <typename T, typename U>
void ExportDBusProperty(ExportedInterface* interface,
                        const std::string& property_name,
                        const Property<U>& property,
                        T (*converter)(const U&),
                        bool force_export) {
  CHECK(interface);
  CHECK(!property_name.empty());

  if (force_export || property.updated()) {
    interface->EnsureExportedPropertyRegistered<T>(property_name)
        ->SetValue(converter(property.value()));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Translation between libnewblue types and newblued types.
////////////////////////////////////////////////////////////////////////////////

// Converts device MAC address (e.g. "00:01:02:03:04:05") to struct bt_addr.
// |result| is valid only if true is returned.
bool ConvertToBtAddr(bool is_random_address,
                     const std::string& addr,
                     struct bt_addr* result);

// Converts the uint8_t[6] MAC address into std::string form, e.g.
// {0x05, 0x04, 0x03, 0x02, 0x01, 0x00} will be 00:01:02:03:04:05.
std::string ConvertBtAddrToString(const struct bt_addr& addr);

// Converts struct uuid to bluetooth::Uuid.
Uuid ConvertToUuid(const struct uuid& uuid);

// Converts bluetooth::Uuid to struct uuid. If |uuid| is invalid, the return
// will be zeros.
struct uuid ConvertToRawUuid(const Uuid& uuid);

// Converts struct GattTraversedService to bluetooth::GattService.
std::unique_ptr<GattService> ConvertToGattService(
    const struct GattTraversedService& service);

// Extracts bytes from |data| without changing the endianess. This does not take
// the ownership of |data|.
std::vector<uint8_t> GetBytesFromSg(const sg data);

void ParseEir(DeviceInfo* device_info, const std::vector<uint8_t>& eir);

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_UTIL_H_
