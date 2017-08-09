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
#include <base/observer_list.h>
#include <gtest/gtest_prod.h>

#include "midis/device.h"
#include "midis/libmidis/clientlib.h"

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
  void EnumerateExistingDevices();

  static uint32_t GenerateDeviceId(uint32_t sys_num, uint32_t device_num) {
    return (sys_num << 8) | device_num;
  }

  virtual std::string ExtractManufacturerString(struct udev_device* udev_device,
                                                const std::string& name);

 private:
  const std::string UdevDeviceGetPropertyOrSysAttr(
      struct udev_device* udev_device, const char* property_key,
      const char* sysattr_key);

  struct UdevDeleter {
    void operator()(udev* dev) const { udev_unref(dev); }
  };

  struct UdevEnumDeleter {
    void operator()(udev_enumerate* enumerate) const {
      udev_enumerate_unref(enumerate);
    }
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
  // TODO(pmalani): Add factory functions that take care of calling
  // initialization routines, like InitDeviceTracker(), so that users of the
  // class don't have to worry about calling a prescribed set of functions.
  DeviceTracker();

  explicit DeviceTracker(std::unique_ptr<UdevHandler> handler);
  void AddDevice(std::unique_ptr<Device> dev);
  void RemoveDevice(uint32_t sys_num, uint32_t dev_num);
  bool InitDeviceTracker();
  void ListDevices(std::vector<MidisDeviceInfo>* list);

  class Observer {
   public:
    virtual ~Observer() {}
    // Function which is executed when a MIDI device is added or removed
    // from the h/w. The client registered as an observer can expect
    // that struct MidisDeviceInfo pointer is allocated and its fields have
    // been filled out correctly.
    //
    // The 'added' argument is set to true if the device was added, and false
    // otherwise.
    virtual void OnDeviceAddedOrRemoved(const struct MidisDeviceInfo* dev_info,
                                        bool added) = 0;
  };

  void AddDeviceObserver(Observer* obs);

  void RemoveDeviceObserver(Observer* obs);

  base::ScopedFD AddClientToReadSubdevice(uint32_t sys_num, uint32_t device_num,
                                          uint32_t subdevice_num,
                                          uint32_t client_id);

  // Remove the client from all watchers for the element of |device_| which
  // matches is identified by |sys_num| and |device_num|. This is useful when a
  // client wants to close requested ports for a device, but may choose to
  // re-request them later on.
  void RemoveClientFromDevice(uint32_t client_id,
                              uint32_t sys_num,
                              uint32_t device_num);

  // Remove the client from all devices in |devices_|. This function is intended
  // to be used when we detect the removal of an entire client either through
  // orderly or disorderly shutdown.
  void RemoveClientFromDevices(uint32_t client_id);

 private:
  friend class ClientTest;
  friend class DeviceTrackerTest;
  FRIEND_TEST(ClientTest, AddClientAndReceiveMessages);
  FRIEND_TEST(DeviceTrackerTest, Add2DevicesPositive);
  FRIEND_TEST(DeviceTrackerTest, AddRemoveDevicePositive);
  FRIEND_TEST(DeviceTrackerTest, AddDeviceRemoveNegative);
  void FillMidisDeviceInfo(const Device* dev, struct MidisDeviceInfo* dev_info);
  void NotifyObserversDeviceAddedOrRemoved(struct MidisDeviceInfo* dev_info,
                                           bool added);

  std::map<uint32_t, std::unique_ptr<Device>> devices_;
  std::unique_ptr<UdevHandler> udev_handler_;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DeviceTracker);
};

}  // namespace midis

#endif  // MIDIS_DEVICE_TRACKER_H_
