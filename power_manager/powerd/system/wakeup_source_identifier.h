// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_WAKEUP_SOURCE_IDENTIFIER_H_
#define POWER_MANAGER_POWERD_SYSTEM_WAKEUP_SOURCE_IDENTIFIER_H_

#include "power_manager/powerd/system/wakeup_source_identifier.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/files/file_path.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/udev_subsystem_observer.h"

namespace power_manager {
namespace system {

struct UdevEvent;
class UdevInterface;
class WakeupDeviceInterface;

// Monitors device sys paths to identify the potential wakeup reason. Monitors
// only if the |wakeup_device_path| points to a directory with power/wakeup
// property.

class WakeupSourceIdentifier : public UdevSubsystemObserver {
 public:
  explicit WakeupSourceIdentifier(UdevInterface* udev);
  ~WakeupSourceIdentifier();

  // Should be called at the beginning of a new suspend request.
  void PrepareForSuspendRequest();

  // Should be called at the end of a suspend request.
  void HandleResume();

  // Returns true if any of the input devices' wakeup counts differed (compared
  // to the pre-suspend wakeup counts).
  bool InputDeviceCausedLastWake() const;

  // UdevSubsystemObserver implementation:
  void OnUdevEvent(const UdevEvent& event) override;

 private:
  // Handles an input being added to or removed from the system.
  void HandleAddedInput(const std::string& input_name,
                        const base::FilePath& wakeup_device_path);
  void HandleRemovedInput(const std::string& input_name);
  UdevInterface* udev_ = nullptr;  // non-owned

  // Keyed by the device sys path. Value is set of input devices
  // names that have this wakeup path.
  using WakeupDeviceMap = std::map<base::FilePath, std::set<std::string>>;
  WakeupDeviceMap wakeup_devices_;

  // Keyed by the device sys path.
  using MonitoredPathsMap =
      std::map<base::FilePath, std::unique_ptr<WakeupDeviceInterface>>;
  MonitoredPathsMap monitored_paths_;

  DISALLOW_COPY_AND_ASSIGN(WakeupSourceIdentifier);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_WAKEUP_SOURCE_IDENTIFIER_H_
