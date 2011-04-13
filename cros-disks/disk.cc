// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "disk.h"

namespace cros_disks {

// Keys that libcros expects to see on the wire. 
// TODO(rtc): We should probably stuff these in a shared header...
const char kDeviceIsDrive[] = "DeviceIsDrive";
const char kDevicePresentationHide[] = "DevicePresentationHide";
const char kDeviceIsMounted[] = "DeviceIsMounted";
const char kDeviceMountPaths[] = "DeviceMountPaths";
const char kDeviceIsMediaAvailable[] = "DeviceIsMediaAvailable";
const char kNativePath[] = "NativePath";
const char kDeviceFile[] = "DeviceFile";
const char kLabel[] = "IdLabel";
const char kDriveModel[] = "DriveModel";
const char kDriveIsRotational[] = "DriveIsRotational";
const char kDeviceIsOpticalDisc[] = "DeviceIsOpticalDisc";
const char kDeviceSize[] = "DeviceSize";
const char kReadOnly[] = "DeviceIsReadOnly";

// TODO(rtc): Figure out what this field is and include it in the response.
const char kPartitionSlave[] = "PartitionSlave";

// TODO(rtc): The constructor should set some defaults, but I'm still iterating
// on the data model.
Disk::Disk() { 
}

Disk::~Disk() {
}

DBusDisk Disk::ToDBusFormat() const {
  DBusDisk disk;
  disk[kDeviceIsDrive].writer().append_bool(is_drive());
  disk[kDevicePresentationHide].writer().append_bool(is_hidden());
  disk[kDeviceIsMounted].writer().append_bool(is_mounted());
  disk[kDeviceMountPaths].writer().append_string(mount_path().c_str());
  disk[kDeviceIsMediaAvailable].writer().append_bool(is_media_available());
  disk[kNativePath].writer().append_string(native_path().c_str());
  disk[kDeviceFile].writer().append_string(device_file().c_str());
  disk[kLabel].writer().append_string(label().c_str());
  disk[kDriveModel].writer().append_string(drive_model().c_str());
  disk[kDriveIsRotational].writer().append_bool(is_rotational());
  disk[kDeviceIsOpticalDisc].writer().append_bool(is_optical_disk());
  disk[kDeviceSize].writer().append_int64(device_capacity());
  disk[kReadOnly].writer().append_bool(is_read_only());
  return disk;
}

} // namespace cros_disks
