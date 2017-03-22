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

#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>

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
  std::string GetName() const { return name_; }
  uint32_t GetCard() const { return card_; }
  uint32_t GetDeviceNum() const { return device_; }

 private:
  friend class DeviceTrackerTest;
  FRIEND_TEST(DeviceTrackerTest, Add2DevicesPositive);
  FRIEND_TEST(DeviceTrackerTest, AddRemoveDevicePositive);
  FRIEND_TEST(DeviceTrackerTest, AddDeviceRemoveNegative);

  std::string name_;
  uint32_t card_;
  uint32_t device_;
  uint32_t num_subdevices_;
  uint32_t flags_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

class UdevHandlerInterface {
 public:
  virtual ~UdevHandlerInterface() = default;
  virtual bool InitUdevHandler() = 0;
  virtual std::string GetMidiDeviceDname(struct udev_device* device) = 0;
  virtual std::unique_ptr<snd_rawmidi_info> GetDeviceInfo(
      const std::string& dname) = 0;
  virtual dev_t GetDeviceDevNum(struct udev_device* device) = 0;
  virtual const char* GetDeviceSysNum(struct udev_device* device) = 0;
};

class DeviceTracker;

class UdevHandler : public UdevHandlerInterface {
 public:
  explicit UdevHandler(DeviceTracker* device_tracker);

  bool InitUdevHandler() override;
  struct udev_device* MonitorReceiveDevice();
  std::string GetMidiDeviceDname(struct udev_device* device) override;
  std::unique_ptr<snd_rawmidi_info> GetDeviceInfo(
      const std::string& dname) override;
  dev_t GetDeviceDevNum(struct udev_device* device) override;
  const char* GetDeviceSysNum(struct udev_device* device) override;
  void ProcessUdevEvent(struct udev_device* device);
  void ProcessUdevFd(int fd);

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

  explicit DeviceTracker(std::unique_ptr<UdevHandlerInterface> handler);
  void AddDevice(struct udev_device* device);
  void RemoveDevice(struct udev_device* device);
  bool InitDeviceTracker();
  static uint32_t GenerateDeviceId(uint32_t sys_num, uint32_t device_num);

 private:
  friend class DeviceTrackerTest;
  FRIEND_TEST(DeviceTrackerTest, Add2DevicesPositive);
  FRIEND_TEST(DeviceTrackerTest, AddRemoveDevicePositive);
  FRIEND_TEST(DeviceTrackerTest, AddDeviceRemoveNegative);

  std::map<uint32_t, std::unique_ptr<Device>> devices_;
  std::unique_ptr<UdevHandlerInterface> udev_handler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceTracker);
};

}  // namespace midis

#endif  // MIDIS_DEVICE_TRACKER_H_
