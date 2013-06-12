// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/udev.h"

#include <libudev.h>

#include <base/format_macros.h>
#include <base/logging.h>
#include <base/stringprintf.h>

#include "mist/udev_device.h"
#include "mist/udev_enumerate.h"
#include "mist/udev_monitor.h"

namespace mist {

Udev::Udev() : udev_(NULL) {}

Udev::~Udev() {
  if (udev_) {
    udev_unref(udev_);
    udev_ = NULL;
  }
}

bool Udev::Initialize() {
  CHECK(!udev_);

  udev_ = udev_new();
  if (udev_)
    return true;

  VLOG(2) << "udev_new() returned NULL.";
  return false;
}

// static
UdevDevice* Udev::CreateDevice(udev_device* device) {
  CHECK(device);

  UdevDevice* device_to_return = new UdevDevice(device);
  CHECK(device_to_return);

  // UdevDevice increases the reference count of the udev_device struct by one.
  // Thus, decrease the reference count of the udev_device struct by one before
  // returning UdevDevice.
  udev_device_unref(device);

  return device_to_return;
}

UdevDevice* Udev::CreateDeviceFromSysPath(const char* sys_path) {
  udev_device* device = udev_device_new_from_syspath(udev_, sys_path);
  if (device)
    return CreateDevice(device);

  VLOG(2) << StringPrintf("udev_device_new_from_syspath"
                          "(%p, \"%s\") returned NULL.",
                          udev_,
                          sys_path);
  return NULL;
}

UdevDevice* Udev::CreateDeviceFromDeviceNumber(char type, dev_t device_number) {
  udev_device* device = udev_device_new_from_devnum(udev_, type, device_number);
  if (device)
    return CreateDevice(device);

  VLOG(2) << StringPrintf("udev_device_new_from_devnum"
                          "(%p, %d, %" PRIu64 ") returned NULL.",
                          udev_,
                          type,
                          device_number);
  return NULL;
}

UdevDevice* Udev::CreateDeviceFromSubsystemSysName(const char* subsystem,
                                                   const char* sys_name) {
  udev_device* device =
      udev_device_new_from_subsystem_sysname(udev_, subsystem, sys_name);
  if (device)
    return CreateDevice(device);

  VLOG(2) << StringPrintf("udev_device_new_from_subsystem_sysname"
                          "(%p, \"%s\", \"%s\") returned NULL.",
                          udev_,
                          subsystem,
                          sys_name);
  return NULL;
}

UdevEnumerate* Udev::CreateEnumerate() {
  udev_enumerate* enumerate = udev_enumerate_new(udev_);
  if (enumerate) {
    UdevEnumerate* enumerate_to_return = new UdevEnumerate(enumerate);
    CHECK(enumerate_to_return);

    // UdevEnumerate increases the reference count of the udev_enumerate struct
    // by one. Thus, decrease the reference count of the udev_enumerate struct
    // by one before returning UdevEnumerate.
    udev_enumerate_unref(enumerate);

    return enumerate_to_return;
  }

  VLOG(2) << StringPrintf("udev_enumerate_new(%p) returned NULL.",
                          udev_);
  return NULL;
}

UdevMonitor* Udev::CreateMonitorFromNetlink(const char* name) {
  udev_monitor* monitor = udev_monitor_new_from_netlink(udev_, name);
  if (monitor) {
    UdevMonitor* monitor_to_return = new UdevMonitor(monitor);
    CHECK(monitor_to_return);

    // UdevMonitor increases the reference count of the udev_monitor struct by
    // one. Thus, decrease the reference count of the udev_monitor struct by one
    // before returning UdevMonitor.
    udev_monitor_unref(monitor);

    return monitor_to_return;
  }

  VLOG(2) << StringPrintf("udev_monitor_new_from_netlink"
                          "(%p, \"%s\") returned NULL.",
                          udev_,
                          name);
  return NULL;
}

}  // namespace mist
