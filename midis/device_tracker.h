// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_DEVICE_TRACKER_H_
#define MIDIS_DEVICE_TRACKER_H_

#include <libudev.h>

#include <map>
#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>

namespace midis {

// Class which holds information related to a MIDI device.
// We use the name variable (derived from the ioctl) as a basis
// to arrive at an identifier.
class Device {
 public:
  Device(const std::string& name,
         uint32_t card,
         uint32_t device,
         uint32_t num_subdevices,
         uint32_t flags);

 private:
  std::string name;
  uint32_t card;
  uint32_t device;
  uint32_t num_subdevices;
  uint32_t flags;
  DISALLOW_COPY_AND_ASSIGN(Device);
};

class DeviceTracker {
 public:
  DeviceTracker();
  virtual ~DeviceTracker();
  bool InitDeviceTracker();

 private:
  void ProcessUdevEvent(struct udev_device* device);
  void ProcessUdevFd(int fd);
  void AddDevice(struct udev_device* device);
  void RemoveDevice(struct udev_device* device);

  struct UdevDeleter {
    void operator()(udev* dev) const { udev_unref(dev); }
  };

  std::unique_ptr<udev, UdevDeleter> udev_;

  struct UdevMonitorDeleter {
    void operator()(udev_monitor* dev) const { udev_monitor_unref(dev); }
  };

  std::unique_ptr<udev_monitor, UdevMonitorDeleter> udev_monitor_;
  base::ScopedFD udev_monitor_fd_;

  std::map<int, std::unique_ptr<Device>> devices_;

  base::WeakPtrFactory<DeviceTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceTracker);
};

}  // namespace midis

#endif  // MIDIS_DEVICE_TRACKER_H_
