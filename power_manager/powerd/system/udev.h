// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_UDEV_H_
#define POWER_MANAGER_POWERD_SYSTEM_UDEV_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/linked_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/observer_list.h>

#include "power_manager/powerd/system/udev_observer.h"

struct udev;
struct udev_device;
struct udev_monitor;

namespace power_manager {
namespace system {

class TaggedDevice;
class TaggedDeviceObserver;

// Watches the udev manager for device-related events (e.g. hotplug).
class UdevInterface {
 public:
  UdevInterface() {}
  virtual ~UdevInterface() {}

  // Adds or removes an observer for watching |subsystem|.
  virtual void AddSubsystemObserver(const std::string& subsystem,
                                    UdevObserver* observer) = 0;
  virtual void RemoveSubsystemObserver(const std::string& subsystem,
                                       UdevObserver* observer) = 0;

  // Adds/removes an observer that will receive events for tagged devices.
  virtual void AddTaggedDeviceObserver(TaggedDeviceObserver* observer) = 0;
  virtual void RemoveTaggedDeviceObserver(TaggedDeviceObserver* observer) = 0;

  // Retrieves a list of all known tagged devices.
  virtual std::vector<TaggedDevice> GetTaggedDevices() = 0;

  // Reads the sysfs attribute |sysattr| from the device specified by |syspath|.
  // Returns true on success. |syspath| is the syspath of a device as returned
  // by libudev, e.g.
  // "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2:1.0/input/input22".
  virtual bool GetSysattr(const std::string& syspath,
                          const std::string& sysattr,
                          std::string* value) = 0;

  // Sets the value of a sysfs attribute. Returns true on success.
  virtual bool SetSysattr(const std::string& syspath,
                          const std::string& sysattr,
                          const std::string& value) = 0;

  // For the device specified by |syspath|, finds the first parent device which
  // has a sysattr named |sysattr|, and stores the parent's syspath in
  // |parent_syspath|. Returs true on success, or false on failure or when no
  // matching parent device was found.
  virtual bool FindParentWithSysattr(const std::string& syspath,
                                     const std::string& sysattr,
                                     std::string* parent_syspath) = 0;
};

// Actual implementation of UdevInterface.
class Udev : public UdevInterface, public base::MessageLoopForIO::Watcher {
 public:
  Udev();
  virtual ~Udev();

  // Initializes the object to listen for events. Returns true on success.
  bool Init();

  // UdevInterface implementation:
  void AddSubsystemObserver(const std::string& subsystem,
                            UdevObserver* observer) override;
  void RemoveSubsystemObserver(const std::string& subsystem,
                               UdevObserver* observer) override;
  void AddTaggedDeviceObserver(TaggedDeviceObserver* observer) override;
  void RemoveTaggedDeviceObserver(TaggedDeviceObserver* observer) override;
  std::vector<TaggedDevice> GetTaggedDevices() override;
  bool GetSysattr(const std::string& syspath,
                  const std::string& sysattr,
                  std::string* value) override;
  bool SetSysattr(const std::string& syspath,
                  const std::string& sysattr,
                  const std::string& value) override;
  bool FindParentWithSysattr(const std::string& syspath,
                             const std::string& sysattr,
                             std::string* parent_syspath) override;

  // base::MessageLoopForIO::Watcher implementation:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  void HandleSubsystemEvent(UdevObserver::Action action,
                            struct udev_device* dev);
  void HandleTaggedDevice(UdevObserver::Action action,
                          struct udev_device* dev);
  void TaggedDeviceChanged(const std::string& syspath,
                           const std::string& tags);
  void TaggedDeviceRemoved(const std::string& syspath);

  // Populates |tagged_devices_| with currently-existing devices.
  bool EnumerateTaggedDevices();

  struct udev* udev_;
  struct udev_monitor* udev_monitor_;

  // Maps from a subsystem name to the corresponding observers.
  typedef std::map<std::string, linked_ptr<ObserverList<UdevObserver> > >
      SubsystemObserverMap;
  SubsystemObserverMap subsystem_observers_;

  ObserverList<TaggedDeviceObserver> tagged_device_observers_;

  // Maps a syspath to the corresponding TaggedDevice.
  std::map<std::string, TaggedDevice> tagged_devices_;

  // Controller for watching |udev_monitor_|'s FD for readability.
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(Udev);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_UDEV_H_
