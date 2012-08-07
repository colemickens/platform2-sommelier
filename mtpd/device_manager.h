// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_DEVICE_MANAGER_H_
#define MTPD_DEVICE_MANAGER_H_

#include <glib.h>
#include <libmtp.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/basictypes.h>

#include "mtpd/storage_info.h"

extern "C" {
struct udev;
struct udev_device;
struct udev_monitor;
}

namespace mtpd {

class DeviceEventDelegate;

class DeviceManager {
 public:
  explicit DeviceManager(DeviceEventDelegate* delegate);
  ~DeviceManager();

  // Returns a file descriptor for monitoring device events.
  int GetDeviceEventDescriptor() const;

  // Processes the available device events.
  void ProcessDeviceEvents();

  // Returns a vector of attached MTP storages.
  std::vector<std::string> EnumerateStorage();

 private:
  // Key: MTP storage id, Value: metadata for the given storage.
  typedef std::map<uint32_t, StorageInfo> MtpStorageMap;
  // (device handle, map of storages on the device)
  typedef std::pair<LIBMTP_mtpdevice_t*, MtpStorageMap> MtpDevice;
  // Key: device bus location, Value: MtpDevice.
  typedef std::map<std::string, MtpDevice> MtpDeviceMap;

  // Callback for udev when something changes for |device|.
  void HandleDeviceNotification(udev_device* device);

  // Iterates through attached devices and find ones that are newly attached.
  // Then populates |device_map_| for the newly attached devices.
  // If this is called as a result of a callback, it came from |source|,
  // which needs to be properly destructed.
  void AddDevices(GSource* source);

  // Iterates through attached devices and find ones that have been detached.
  // Then removes the detached devices from |device_map_|.
  // If |remove_all| is true, then assumes all devices have been detached.
  void RemoveDevices(bool remove_all);

  // libudev-related items: the main context, the monitoring context to be
  // notified about changes to device states, and the monitoring context's
  // file descriptor.
  udev* udev_;
  udev_monitor* udev_monitor_;
  int udev_monitor_fd_;

  DeviceEventDelegate* delegate_;

  // Map of devices and storages.
  MtpDeviceMap device_map_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace mtpd

#endif  // MTPD_DEVICE_MANAGER_H_
