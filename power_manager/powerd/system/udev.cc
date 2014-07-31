// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/udev.h"

#include <libudev.h>

#include <utility>

#include <base/logging.h>

#include "power_manager/powerd/system/udev_observer.h"

namespace power_manager {
namespace system {

Udev::Udev() : udev_(NULL), udev_monitor_(NULL) {}

Udev::~Udev() {
  if (udev_monitor_)
    udev_monitor_unref(udev_monitor_);
  if (udev_)
    udev_unref(udev_);
}

bool Udev::Init() {
  udev_ = udev_new();
  if (!udev_) {
    PLOG(ERROR) << "udev_new() failed";
    return false;
  }

  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!udev_monitor_) {
    PLOG(ERROR) << "udev_monitor_new_from_netlink() failed";
    return false;
  }

  udev_monitor_enable_receiving(udev_monitor_);

  int fd = udev_monitor_get_fd(udev_monitor_);
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          fd, true, base::MessageLoopForIO::WATCH_READ, &watcher_, this)) {
    LOG(ERROR) << "Unable to watch FD " << fd;
    return false;
  }

  LOG(INFO) << "Watching FD " << fd << " for udev events";
  return true;
}

void Udev::AddObserver(const std::string& subsystem,
                       UdevObserver* observer) {
  CHECK(udev_) << "Object uninitialized";
  DCHECK(observer);
  ObserverMap::iterator it = subsystem_observers_.find(subsystem);
  if (it == subsystem_observers_.end()) {
    VLOG(1) << "Adding match for subsystem \"" << subsystem << "\"";
    if (udev_monitor_filter_add_match_subsystem_devtype(
            udev_monitor_, subsystem.c_str(), NULL) != 0)
      LOG(ERROR) << "Unable to add match for subsystem \"" << subsystem << "\"";
    if (udev_monitor_filter_update(udev_monitor_))
      LOG(ERROR) << "udev_monitor_filter_update failed";
    it = subsystem_observers_.insert(std::make_pair(
        subsystem, make_linked_ptr(new ObserverList<UdevObserver>))).first;
  }
  it->second->AddObserver(observer);
}

void Udev::RemoveObserver(const std::string& subsystem,
                          UdevObserver* observer) {
  DCHECK(observer);
  ObserverMap::iterator it = subsystem_observers_.find(subsystem);
  if (it != subsystem_observers_.end())
    it->second->RemoveObserver(observer);
  // No way to remove individual matches from udev_monitor. :-/
}

bool Udev::GetSysattr(const std::string& syspath,
                      const std::string& sysattr,
                      std::string* value) {
  DCHECK(udev_);
  DCHECK(value);
  value->clear();

  struct udev_device* device =
      udev_device_new_from_syspath(udev_, syspath.c_str());
  if (!device) {
    LOG(WARNING) << "Failed to open udev device: " << syspath;
    return false;
  }
  const char* value_cstr =
      udev_device_get_sysattr_value(device, sysattr.c_str());
  if (value_cstr)
    *value = value_cstr;
  udev_device_unref(device);
  return value_cstr != NULL;
}

bool Udev::SetSysattr(const std::string& syspath,
                      const std::string& sysattr,
                      const std::string& value) {
  DCHECK(udev_);

  struct udev_device* device =
      udev_device_new_from_syspath(udev_, syspath.c_str());
  if (!device) {
    LOG(WARNING) << "Failed to open udev device: " << syspath;
    return false;
  }
  // udev can modify this value, hence we copy it first.
  scoped_ptr<char, base::FreeDeleter> value_mutable(strdup(value.c_str()));
  int rv = udev_device_set_sysattr_value(device, sysattr.c_str(),
                                         value_mutable.get());
  udev_device_unref(device);
  if (rv != 0) {
    LOG(WARNING) << "Failed to set sysattr '" << sysattr << "' on device "
                 << syspath << ": " << strerror(-rv);
    return false;
  }
  return true;
}

bool Udev::FindParentWithSysattr(const std::string& syspath,
                                 const std::string& sysattr,
                                 std::string* parent_syspath) {
  DCHECK(udev_);

  struct udev_device* device =
      udev_device_new_from_syspath(udev_, syspath.c_str());
  if (!device) {
    LOG(WARNING) << "Failed to open udev device: " << syspath;
    return false;
  }
  struct udev_device* parent = device;
  while (parent) {
    const char* value = udev_device_get_sysattr_value(parent, sysattr.c_str());
    if (value)
      break;
    // Go up one level.
    parent = udev_device_get_parent(parent);
  }
  if (parent)
    *parent_syspath = udev_device_get_syspath(parent);
  udev_device_unref(device);
  return parent != NULL;
}

void Udev::OnFileCanReadWithoutBlocking(int fd) {
  struct udev_device* dev = udev_monitor_receive_device(udev_monitor_);
  if (!dev)
    return;

  const char* subsystem = udev_device_get_subsystem(dev);
  const char* sysname = udev_device_get_sysname(dev);
  const char* action_str = udev_device_get_action(dev);
  VLOG(1) << "Received event: subsystem=" << subsystem
          << " sysname=" << sysname << " action=" << action_str;

  UdevObserver::Action action = UdevObserver::ACTION_UNKNOWN;
  if (action_str) {
    if (strcmp(action_str, "add") == 0)
      action = UdevObserver::ACTION_ADD;
    else if (strcmp(action_str, "remove") == 0)
      action = UdevObserver::ACTION_REMOVE;
    else if (strcmp(action_str, "change") == 0)
      action = UdevObserver::ACTION_CHANGE;
    else if (strcmp(action_str, "online") == 0)
      action = UdevObserver::ACTION_ONLINE;
    else if (strcmp(action_str, "offline") == 0)
      action = UdevObserver::ACTION_OFFLINE;
  }

  ObserverMap::iterator it = subsystem_observers_.find(subsystem);
  if (it != subsystem_observers_.end()) {
    ObserverList<UdevObserver>* observers = it->second.get();
    FOR_EACH_OBSERVER(UdevObserver, *observers,
        OnUdevEvent(subsystem, sysname ? sysname : "", action));
  }

  udev_device_unref(dev);
}

void Udev::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Unexpected non-blocking write notification for FD " << fd;
}

}  // namespace system
}  // namespace power_manager
