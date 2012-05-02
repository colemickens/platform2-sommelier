// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk.h"

#include <algorithm>

using std::string;

namespace {

const char kUSBDriveName[] = "USB Drive";
const char kSDCardName[] = "SD Card";
const char kOpticalDiscName[] = "Optical Disc";
const char kMobileDeviceName[] = "Mobile Device";
const char kDVDName[] = "DVD";
const char kFallbackPresentationName[] = "External Drive";

}  // namespace

namespace cros_disks {

Disk::Disk()
    : is_drive_(false),
      is_hidden_(false),
      is_auto_mountable_(false),
      is_mounted_(false),
      is_media_available_(false),
      is_on_boot_device_(true),
      is_rotational_(false),
      is_read_only_(false),
      is_virtual_(true),
      mount_paths_(),
      native_path_(),
      device_file_(),
      uuid_(),
      label_(),
      drive_model_(),
      media_type_(DEVICE_MEDIA_UNKNOWN),
      device_capacity_(0),
      bytes_remaining_(0) {
}

Disk::~Disk() {
}

string Disk::GetPresentationName() const {
  if (!label_.empty()) {
    string name = label_;
    std::replace(name.begin(), name.end(), '/', '_');
    return name;
  }

  switch (media_type_) {
    case DEVICE_MEDIA_USB:
      return kUSBDriveName;
    case DEVICE_MEDIA_SD:
      return kSDCardName;
    case DEVICE_MEDIA_OPTICAL_DISC:
      return kOpticalDiscName;
    case DEVICE_MEDIA_MOBILE:
      return kMobileDeviceName;
    case DEVICE_MEDIA_DVD:
      return kDVDName;
    default:
      return kFallbackPresentationName;
  }
}

DBusDisk Disk::ToDBusFormat() const {
  DBusDisk disk;
  disk[kDeviceIsDrive].writer().append_bool(is_drive());
  disk[kDevicePresentationHide].writer().append_bool(is_hidden());
  disk[kDeviceIsMounted].writer().append_bool(is_mounted());
  disk[kDeviceIsMediaAvailable].writer().append_bool(is_media_available());
  disk[kDeviceIsOnBootDevice].writer().append_bool(is_on_boot_device());
  disk[kDeviceIsVirtual].writer().append_bool(is_virtual());
  disk[kNativePath].writer().append_string(native_path().c_str());
  disk[kDeviceFile].writer().append_string(device_file().c_str());
  disk[kIdUuid].writer().append_string(uuid().c_str());
  disk[kIdLabel].writer().append_string(label().c_str());
  disk[kDriveModel].writer().append_string(drive_model().c_str());
  disk[kDriveIsRotational].writer().append_bool(is_rotational());
  disk[kDeviceMediaType].writer().append_uint32(media_type());
  disk[kDeviceSize].writer().append_uint64(device_capacity());
  disk[kDeviceIsReadOnly].writer().append_bool(is_read_only());
  DBus::MessageIter iter = disk[kDeviceMountPaths].writer();
  iter << mount_paths();
  return disk;
}

}  // namespace cros_disks
