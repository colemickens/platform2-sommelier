// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_DEVICE_TRACKER_H_
#define MIDIS_DEVICE_TRACKER_H_

#include <libudev.h>
#include <sound/asound.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>

#include "midis/device.h"
#include "midis/messages.h"

namespace midis {

class DeviceTracker;

class UdevHandler {
 public:
  explicit UdevHandler(DeviceTracker* device_tracker);
  virtual ~UdevHandler() {}

  bool InitUdevHandler();
  std::unique_ptr<Device> CreateDevice(struct udev_device* udev_device);

  struct UdevDeviceDeleter {
    void operator()(udev_device* dev) const { udev_device_unref(dev); }
  };

  virtual std::string GetMidiDeviceDname(struct udev_device* device);
  virtual std::unique_ptr<snd_rawmidi_info> GetDeviceInfo(
      const std::string& dname);
  dev_t GetDeviceDevNum(struct udev_device* udev_device);
  const char* GetDeviceSysNum(struct udev_device* udev_device);
  void ProcessUdevEvent(struct udev_device* udev_device);
  void ProcessUdevFd(int fd);

  static uint32_t GenerateDeviceId(uint32_t sys_num, uint32_t device_num) {
    return (sys_num << 8) | device_num;
  }

 private:
  struct UdevDeleter {
    void operator()(udev* dev) const { udev_unref(dev); }
  };

  std::unique_ptr<udev, UdevDeleter> udev_;

  struct UdevMonitorDeleter {
    void operator()(udev_monitor* dev) const { udev_monitor_unref(dev); }
  };

  std::unique_ptr<udev_monitor, UdevMonitorDeleter> udev_monitor_;

  base::ScopedFD udev_monitor_fd_;
  DeviceTracker* dev_tracker_;
  base::WeakPtrFactory<UdevHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UdevHandler);
};

class DeviceTracker {
 public:
  DeviceTracker();

  explicit DeviceTracker(std::unique_ptr<UdevHandler> handler);
  void AddDevice(std::unique_ptr<Device> dev);
  void RemoveDevice(uint32_t sys_num, uint32_t dev_num);
  bool InitDeviceTracker();
  void ListDevices(std::vector<MidisDeviceInfo>* list);

 private:
  friend class DeviceTrackerTest;
  FRIEND_TEST(DeviceTrackerTest, Add2DevicesPositive);
  FRIEND_TEST(DeviceTrackerTest, AddRemoveDevicePositive);
  FRIEND_TEST(DeviceTrackerTest, AddDeviceRemoveNegative);

  std::map<uint32_t, std::unique_ptr<Device>> devices_;
  std::unique_ptr<UdevHandler> udev_handler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceTracker);
};

}  // namespace midis

#endif  // MIDIS_DEVICE_TRACKER_H_
