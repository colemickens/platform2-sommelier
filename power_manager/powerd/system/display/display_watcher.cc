// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display/display_watcher.h"

#include <algorithm>
#include <string>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>

#include "power_manager/powerd/system/udev.h"

namespace power_manager {
namespace system {

namespace {

// Path containing directories describing the state of DRM devices.
const char kSysClassDrmPath[] = "/sys/class/drm";

// Glob-style pattern for device directories within kSysClassDrmPath.
const char kDrmDeviceNamePattern[] = "card*";

// Glob-style pattern for the I2C device name within a DRM device directory.
const char kI2CDeviceNamePattern[] = "i2c-*";

// Directory containing I2C devices.
const char kI2CDevPath[] = "/dev";

// Returns true if the device described by |drm_device_dir| is connected.
bool DeviceIsConnected(const base::FilePath& drm_device_dir) {
  base::FilePath status_path =
      drm_device_dir.Append(DisplayWatcher::kDrmStatusFile);
  std::string status;
  if (!base::ReadFileToString(status_path, &status))
    return false;

  // Trim whitespace to deal with trailing newlines.
  base::TrimWhitespaceASCII(status, base::TRIM_TRAILING, &status);
  return status == DisplayWatcher::kDrmStatusConnected;
}

}  // namespace

const char DisplayWatcher::kI2CUdevSubsystem[] = "i2c-dev";
const char DisplayWatcher::kDrmUdevSubsystem[] = "drm";
const char DisplayWatcher::kDrmStatusFile[] = "status";
const char DisplayWatcher::kDrmStatusConnected[] = "connected";

DisplayWatcher::DisplayWatcher() : udev_(nullptr) {}

DisplayWatcher::~DisplayWatcher() {
  if (udev_) {
    udev_->RemoveSubsystemObserver(kI2CUdevSubsystem, this);
    udev_->RemoveSubsystemObserver(kDrmUdevSubsystem, this);
    udev_ = nullptr;
  }
}

void DisplayWatcher::Init(UdevInterface* udev) {
  udev_ = udev;
  udev_->AddSubsystemObserver(kI2CUdevSubsystem, this);
  udev_->AddSubsystemObserver(kDrmUdevSubsystem, this);
  UpdateDisplays();
}

const std::vector<DisplayInfo>& DisplayWatcher::GetDisplays() const {
  return displays_;
}

void DisplayWatcher::AddObserver(DisplayWatcherObserver* observer) {
  observers_.AddObserver(observer);
}

void DisplayWatcher::RemoveObserver(DisplayWatcherObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DisplayWatcher::OnUdevEvent(const UdevEvent& event) {
  VLOG(1) << "Got udev event for " << event.device_info.sysname
          << " on subsystem " << event.device_info.subsystem;
  UpdateDisplays();
}

base::FilePath DisplayWatcher::GetI2CDevicePath(const base::FilePath& drm_dir) {
  base::FileEnumerator enumerator(drm_dir,
                                  false,  // recursive
                                  base::FileEnumerator::DIRECTORIES,
                                  kI2CDeviceNamePattern);
  for (base::FilePath i2c_dir = enumerator.Next(); !i2c_dir.empty();
       i2c_dir = enumerator.Next()) {
    base::FilePath dev_path = i2c_dev_path_for_testing_.empty()
                                  ? base::FilePath(kI2CDevPath)
                                  : i2c_dev_path_for_testing_;
    const std::string i2c_name = i2c_dir.BaseName().value();
    base::FilePath i2c_dev = dev_path.Append(i2c_name);
    if (base::PathExists(i2c_dev))
      return i2c_dev;
  }
  return base::FilePath();
}

void DisplayWatcher::UpdateDisplays() {
  std::vector<DisplayInfo> new_displays;

  base::FileEnumerator enumerator(
      sysfs_drm_path_for_testing_.empty() ? base::FilePath(kSysClassDrmPath)
                                          : sysfs_drm_path_for_testing_,
      false,  // recursive
      static_cast<base::FileEnumerator::FileType>(
          base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS),
      kDrmDeviceNamePattern);
  for (base::FilePath device_path = enumerator.Next(); !device_path.empty();
       device_path = enumerator.Next()) {
    if (!DeviceIsConnected(device_path))
      continue;

    DisplayInfo info;
    info.drm_path = device_path;
    info.i2c_path = GetI2CDevicePath(device_path);
    new_displays.push_back(info);
    VLOG(1) << "Found connected display: drm_path=" << info.drm_path.value()
            << ", i2c_path=" << info.i2c_path.value();
  }

  std::sort(new_displays.begin(), new_displays.end());
  if (new_displays != displays_) {
    displays_.swap(new_displays);
    for (DisplayWatcherObserver& observer : observers_)
      observer.OnDisplaysChanged(displays_);
  }
}

}  // namespace system
}  // namespace power_manager
