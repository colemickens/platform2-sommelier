// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_UDEV_H_
#define MIST_UDEV_H_

#include <sys/types.h>

#include <base/basictypes.h>

struct udev;
struct udev_device;

namespace mist {

class UdevDevice;
class UdevEnumerate;
class UdevMonitor;

// A udev library context, which wraps a udev C struct from libudev and related
// library functions into a C++ object.
class Udev {
 public:
  Udev();
  virtual ~Udev();

  // Initializes this object with a udev library context created via udev_new().
  // Returns true on success.
  virtual bool Initialize();

  // Wraps udev_device_new_from_syspath(). The returned UdevDevice object is not
  // managed and should be deleted by the caller after use.
  virtual UdevDevice* CreateDeviceFromSysPath(const char* sys_path);

  // Wraps udev_device_new_from_devnum(). The returned UdevDevice object is not
  // managed and should be deleted by the caller after use.
  virtual UdevDevice* CreateDeviceFromDeviceNumber(char type,
                                                   dev_t device_number);

  // Wraps udev_device_new_from_subsystem_sysname(). The returned UdevDevice
  // object is not managed and should be deleted by the caller after use.
  virtual UdevDevice* CreateDeviceFromSubsystemSysName(const char* subsystem,
                                                       const char* sys_name);

  // Wraps udev_enumerate_new(). The returned UdevEnumerate object is not
  // managed and should be deleted by the caller after use.
  virtual UdevEnumerate* CreateEnumerate();

  // Wraps udev_monitor_new_from_netlink(). The returned UdevMonitor object is
  // not managed and should be deleted by the caller after use.
  virtual UdevMonitor* CreateMonitorFromNetlink(const char* name);

 private:
  // Creates a UdevDevice object that wraps a given udev_device struct pointed
  // by |device|. The ownership of |device| is transferred to returned
  // UdevDevice object.
  static UdevDevice* CreateDevice(udev_device* device);

  udev* udev_;

  DISALLOW_COPY_AND_ASSIGN(Udev);
};

}  // namespace mist

#endif  // MIST_UDEV_H_
