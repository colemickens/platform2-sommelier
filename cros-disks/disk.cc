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
const char kPartitionSlave[] = "PartitionSlave";
const char kDriveIsRotational[] = "DriveIsRotational";
const char kDeviceIsOpticalDisc[] = "DeviceIsOpticalDisc";
const char kDeviceSize[] = "DeviceSize";
const char kReadOnly[] = "DeviceIsReadOnly";


// TODO(rtc): The constructor should set some defaults, but I'm still iterating
// on the data model.
Disk::Disk() { 
}

Disk::~Disk() {
}

} // namespace cros_disks
