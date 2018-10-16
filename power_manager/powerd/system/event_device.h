// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_H_
#define POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_H_

#include "power_manager/powerd/system/event_device_interface.h"

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/memory/linked_ptr.h>
#include <base/message_loop/message_loop.h>

namespace power_manager {
namespace system {

// Real implementation of EventDeviceInterface.
class EventDevice : public EventDeviceInterface,
                    public base::MessageLoopForIO::Watcher {
 public:
  EventDevice(int fd, const base::FilePath& path);
  ~EventDevice() override;

  // Implementation of EventDeviceInterface.
  std::string GetDebugName() override;
  std::string GetName() override;
  std::string GetPhysPath() override;
  bool IsCrosFp() override;
  bool IsLidSwitch() override;
  bool IsTabletModeSwitch() override;
  bool IsPowerButton() override;
  bool HoverSupported() override;
  bool HasLeftButton() override;
  LidState GetInitialLidState() override;
  TabletMode GetInitialTabletMode() override;
  bool ReadEvents(std::vector<input_event>* events_out) override;
  void WatchForEvents(base::Closure new_events_cb) override;

  // Implementation of base::MessageLoopForIO::Watcher.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  // Checks whether bit index |bit| is set in the bitmask returned by
  // EVIOCGBIT(|event_type|).
  bool HasEventBit(int event_type, int bit);

  // Fetches the state of a single switch by using EVIOCGSW. |bit| is one of the
  // SW_* constants.
  bool GetSwitchBit(int bit);

  int fd_;
  base::FilePath path_;
  base::Closure new_events_cb_;
  std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  DISALLOW_COPY_AND_ASSIGN(EventDevice);
};

class EventDeviceFactory : public EventDeviceFactoryInterface {
 public:
  EventDeviceFactory();
  ~EventDeviceFactory() override;

  // Implementation of EventDeviceFactoryInterface.
  linked_ptr<EventDeviceInterface> Open(const base::FilePath& path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventDeviceFactory);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_H_
