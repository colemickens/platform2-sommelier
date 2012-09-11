// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DISK_H_
#define CROS_DISKS_DISK_H_

#include <dbus-c++/dbus.h>  // NOLINT

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/dbus/service_constants.h>

namespace cros_disks {

typedef std::map<std::string, DBus::Variant> DBusDisk;
typedef std::vector<DBusDisk> DBusDisks;

// A simple type that describes a storage device attached to our system.
//
// This class was designed to run in a single threaded context and should not
// be considered thread safe.
class Disk {
 public:
  Disk();
  virtual ~Disk();

  // Returns a presentation name of the disk, which can be used to name
  // the mount directory of the disk. The naming scheme is as follows:
  // (1) Use a non-empty label if the disk has one.
  // (2) Otherwise, use one of the following names based on the device
  //     media type:
  //     - USB drive
  //     - SD card
  //     - Optical disc
  //     - Mobile device
  //     - External drive (if the device media type is unknown)
  // Any forward slash '/' in the presentation name is replaced with an
  // underscore '_'.
  std::string GetPresentationName() const;

  DBusDisk ToDBusFormat() const;

  bool is_drive() const { return is_drive_; }
  void set_is_drive(bool is_drive) { is_drive_ = is_drive; }

  bool is_hidden() const { return is_hidden_; }
  void set_is_hidden(bool is_hidden) { is_hidden_ = is_hidden; }

  bool is_auto_mountable() const { return is_auto_mountable_; }
  void set_is_auto_mountable(bool is_auto_mountable) {
    is_auto_mountable_ = is_auto_mountable;
  }

  bool is_mounted() const { return is_mounted_; }
  void set_is_mounted(bool is_mounted) { is_mounted_ = is_mounted; }

  bool is_media_available() const { return is_media_available_; }
  void set_is_media_available(bool is_media_available) {
    is_media_available_ = is_media_available;
  }

  bool is_on_boot_device() const { return is_on_boot_device_; }
  void set_is_on_boot_device(bool is_on_boot_device) {
    is_on_boot_device_ = is_on_boot_device;
  }

  bool is_rotational() const { return is_rotational_; }
  void set_is_rotational(bool is_rotational) { is_rotational_ = is_rotational; }

  bool is_optical_disk() const {
    return (media_type_ == DEVICE_MEDIA_OPTICAL_DISC ||
            media_type_ == DEVICE_MEDIA_DVD);
  }

  bool is_read_only() const { return is_read_only_; }
  void set_is_read_only(bool is_read_only) { is_read_only_ = is_read_only; }

  bool is_virtual() const { return is_virtual_; }
  void set_is_virtual(bool is_virtual) { is_virtual_ = is_virtual; }

  const std::vector<std::string>& mount_paths() const { return mount_paths_; }
  void set_mount_paths(const std::vector<std::string>& mount_paths) {
    mount_paths_ = mount_paths;
  }

  std::string native_path() const { return native_path_; }
  void set_native_path(const std::string& native_path) {
    native_path_ = native_path;
  }

  std::string device_file() const { return device_file_; }
  void set_device_file(const std::string& device_file) {
    device_file_ = device_file;
  }

  std::string filesystem_type() const { return filesystem_type_; }
  void set_filesystem_type(const std::string& filesystem_type) {
    filesystem_type_ = filesystem_type;
  }

  std::string uuid() const { return uuid_; }
  void set_uuid(const std::string& uuid) { uuid_ = uuid; }

  std::string label() const { return label_; }
  void set_label(const std::string& label) { label_ = label; }

  std::string vendor_id() const { return vendor_id_; }
  void set_vendor_id(const std::string& vendor_id) {
    vendor_id_ = vendor_id;
  }

  std::string vendor_name() const { return vendor_name_; }
  void set_vendor_name(const std::string& vendor_name) {
    vendor_name_ = vendor_name;
  }

  std::string product_id() const { return product_id_; }
  void set_product_id(const std::string& product_id) {
    product_id_ = product_id;
  }

  std::string product_name() const { return product_name_; }
  void set_product_name(const std::string& product_name) {
    product_name_ = product_name;
  }

  std::string drive_model() const { return drive_model_; }
  void set_drive_model(const std::string& drive_model) {
    drive_model_ = drive_model;
  }

  DeviceMediaType media_type() const { return media_type_; }
  void set_media_type(DeviceMediaType media_type) { media_type_ = media_type; }

  uint64 device_capacity() const { return device_capacity_; }
  void set_device_capacity(uint64 device_capacity) {
    device_capacity_ = device_capacity;
  }

  uint64 bytes_remaining() const { return bytes_remaining_; }
  void set_bytes_remaining(uint64 bytes_remaining) {
    bytes_remaining_ = bytes_remaining;
  }

 private:
  bool is_drive_;
  bool is_hidden_;
  bool is_auto_mountable_;
  bool is_mounted_;
  bool is_media_available_;
  bool is_on_boot_device_;
  bool is_rotational_;
  bool is_read_only_;
  bool is_virtual_;
  std::vector<std::string> mount_paths_;
  std::string native_path_;
  std::string device_file_;
  std::string filesystem_type_;
  std::string uuid_;
  std::string label_;
  std::string vendor_id_;
  std::string vendor_name_;
  std::string product_id_;
  std::string product_name_;
  std::string drive_model_;
  DeviceMediaType media_type_;
  uint64 device_capacity_;
  uint64 bytes_remaining_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DISK_H_
