// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk.h"

#include <algorithm>

using std::string;

namespace cros_disks {

static const char kFallbackPresentationName[] = "Untitled";

// Keys that libcros expects to see on the wire.
// TODO(rtc): We should probably stuff these in a shared header...
static const char kDeviceIsDrive[] = "DeviceIsDrive";
static const char kDevicePresentationHide[] = "DevicePresentationHide";
static const char kDeviceIsMounted[] = "DeviceIsMounted";
static const char kDeviceMountPaths[] = "DeviceMountPaths";
static const char kDeviceIsMediaAvailable[] = "DeviceIsMediaAvailable";
static const char kDeviceIsOnBootDevice[] = "DeviceIsOnBootDevice";
static const char kDeviceIsVirtual[] = "DeviceIsVirtual";
static const char kNativePath[] = "NativePath";
static const char kDeviceFile[] = "DeviceFile";
static const char kUuid[] = "IdUuid";
static const char kLabel[] = "IdLabel";
static const char kDriveModel[] = "DriveModel";
static const char kDriveIsRotational[] = "DriveIsRotational";
static const char kDeviceIsOpticalDisc[] = "DeviceIsOpticalDisc";
static const char kDeviceSize[] = "DeviceSize";
static const char kReadOnly[] = "DeviceIsReadOnly";

// TODO(rtc): Figure out what this field is and include it in the response.
static const char kPartitionSlave[] = "PartitionSlave";

Disk::Disk()
  : is_drive_(false),
    is_hidden_(false),
    is_auto_mountable_(false),
    is_mounted_(false),
    is_media_available_(false),
    is_on_boot_device_(true),
    is_rotational_(false),
    is_optical_disk_(false),
    is_read_only_(false),
    is_virtual_(true),
    mount_paths_(),
    native_path_(),
    device_file_(),
    uuid_(),
    label_(),
    drive_model_(),
    device_capacity_(0),
    bytes_remaining_(0) {
}

Disk::~Disk() {
}

string Disk::GetPresentationName() const {
  string name;
  if (!label_.empty())
    name = label_;
  else if (!uuid_.empty())
    name = uuid_;
  else
    name = kFallbackPresentationName;

  std::replace(name.begin(), name.end(), '/', '_');
  return name;
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
  disk[kUuid].writer().append_string(uuid().c_str());
  disk[kLabel].writer().append_string(label().c_str());
  disk[kDriveModel].writer().append_string(drive_model().c_str());
  disk[kDriveIsRotational].writer().append_bool(is_rotational());
  disk[kDeviceIsOpticalDisc].writer().append_bool(is_optical_disk());
  disk[kDeviceSize].writer().append_uint64(device_capacity());
  disk[kReadOnly].writer().append_bool(is_read_only());
  DBus::MessageIter iter = disk[kDeviceMountPaths].writer();
  iter << mount_paths();
  return disk;
}

}  // namespace cros_disks
