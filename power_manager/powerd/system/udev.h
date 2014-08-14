// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_UDEV_H_
#define POWER_MANAGER_POWERD_SYSTEM_UDEV_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/memory/linked_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/observer_list.h>

struct udev;
struct udev_monitor;

namespace power_manager {
namespace system {

class UdevObserver;

// Watches the udev manager for device-related events (e.g. hotplug).
class UdevInterface {
 public:
  UdevInterface() {}
  virtual ~UdevInterface() {}

  // Adds or removes an observer for watching |subsystem|.
  virtual void AddObserver(const std::string& subsystem,
                           UdevObserver* observer) = 0;
  virtual void RemoveObserver(const std::string& subsystem,
                              UdevObserver* observer) = 0;
};

// Actual implementation of UdevInterface.
class Udev : public UdevInterface, public base::MessageLoopForIO::Watcher {
 public:
  Udev();
  virtual ~Udev();

  // Initializes the object to listen for events. Returns true on success.
  bool Init();

  // UdevInterface implementation:
  void AddObserver(const std::string& subsystem,
                   UdevObserver* observer) override;
  void RemoveObserver(const std::string& subsystem,
                      UdevObserver* observer) override;

  // base::MessageLoopForIO::Watcher implementation:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  struct udev* udev_;
  struct udev_monitor* udev_monitor_;

  // Maps from a subsystem name to the corresponding observers.
  typedef std::map<std::string, linked_ptr<ObserverList<UdevObserver> > >
      ObserverMap;
  ObserverMap subsystem_observers_;

  // Controller for watching |udev_monitor_|'s FD for readability.
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(Udev);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_UDEV_H_
