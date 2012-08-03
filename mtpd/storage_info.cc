// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/storage_info.h"

#include "string_helpers.h"

namespace mtpd {

StorageInfo::StorageInfo(const LIBMTP_device_entry_t& device,
                         const LIBMTP_devicestorage_t& storage)
    : vendor_id_(device.vendor_id),
      product_id_(device.product_id),
      device_flags_(device.device_flags),
      storage_type_(storage.StorageType),
      filesystem_type_(storage.FilesystemType),
      access_capability_(storage.AccessCapability),
      max_capacity_(storage.MaxCapacity),
      free_space_in_bytes_(storage.FreeSpaceInBytes),
      free_space_in_objects_(storage.FreeSpaceInObjects) {
  if (device.vendor)
    vendor_ = device.vendor;
  if (device.product)
    product_ = device.product;
  if (storage.StorageDescription)
    storage_description_ = storage.StorageDescription;
  if (storage.VolumeIdentifier)
    volume_identifier_ = storage.VolumeIdentifier;
}

StorageInfo::~StorageInfo() {
}

DBusMTPStorage StorageInfo::ToDBusFormat() const {
  // TODO(thestig) Move string constants to system_api/dbus/service_constants.h.
  DBusMTPStorage entry;
  entry["Vendor"].writer().append_string(EnsureUTF8String(vendor_).c_str());
  entry["VendorId"].writer().append_uint16(vendor_id_);
  entry["Product"].writer().append_string(EnsureUTF8String(product_).c_str());
  entry["ProductId"].writer().append_uint16(product_id_);
  entry["DeviceFlags"].writer().append_uint32(device_flags_);
  entry["StorageType"].writer().append_uint16(storage_type_);
  entry["FilesystemType"].writer().append_uint16(filesystem_type_);
  entry["AccessCapability"].writer().append_uint16(access_capability_);
  entry["MaxCapacity"].writer().append_uint64(max_capacity_);
  entry["FreeSpaceInBytes"].writer().append_uint64(free_space_in_bytes_);
  entry["FreeSpaceInObjects"].writer().append_uint64(free_space_in_objects_);
  entry["StorageDescription"].writer().append_string(
      EnsureUTF8String(storage_description_).c_str());
  entry["VolumeIdentifier"].writer().append_string(
      EnsureUTF8String(volume_identifier_).c_str());
  return entry;
}

}  // namespace mtpd
