// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SERVICE_CONSTANTS_H_
#define CROS_DISKS_SERVICE_CONSTANTS_H_

namespace cros_disks {

// TODO(benchan): Once these service constants become stable enough,
// move them to system_api/dbus/service_constants.

enum DeviceMediaType {
  kDeviceMediaUnknown = 0,
  kDeviceMediaUSB = 1,
  kDeviceMediaSD = 2,
  kDeviceMediaOpticalDisc = 3,
  kDeviceMediaMobile = 4,
};

enum MountSourceType {
  kMountSourceInvalid = 0,
  kMountSourceRemovableDevice = 1,
  kMountSourceArchive = 2,
  kMountSourceNetworkStorage = 3,
};

enum MountErrorType {
  kMountErrorNone = 0,
  kMountErrorUnknown = 1,
  kMountErrorInternal = 2,
  kMountErrorInvalidArgument = 3,
  kMountErrorInvalidPath = 4,
  kMountErrorPathAlreadyMounted = 5,
  kMountErrorPathNotMounted = 6,
  kMountErrorDirectoryCreationFailed = 7,
  kMountErrorInvalidMountOptions = 8,
  kMountErrorInvalidUnmountOptions = 9,
  kMountErrorInsufficientPermissions = 10,
  kMountErrorMountProgramNotFound = 11,
  kMountErrorMountProgramFailed = 12,
  kMountErrorInvalidDevicePath = 100,
  kMountErrorUnknownFilesystem = 101,
  kMountErrorUnsupportedFilesystem = 102,
  kMountErrorInvalidArchive = 201,
  kMountErrorUnsupportedArchive = 202,
  // TODO(benchan): Add more error types.
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SERVICE_CONSTANTS_H_
