// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_PERIPHERAL_BATTERY_WATCHER_H_
#define POWER_MANAGER_POWERD_SYSTEM_PERIPHERAL_BATTERY_WATCHER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "power_manager/powerd/system/async_file_reader.h"

namespace power_manager {

class DBusSenderInterface;

namespace system {

class PeripheralBatteryWatcher {
 public:
  PeripheralBatteryWatcher();
  ~PeripheralBatteryWatcher();

  void set_battery_path_for_testing(const base::FilePath& path) {
    peripheral_battery_path_ = path;
  }

  // Starts polling.
  void Init(DBusSenderInterface* dbus_sender);

 private:
  // Handler for a periodic event that reads the peripheral batteries'
  // level.
  void ReadBatteryStatuses();

  // Fills |battery_list| with paths containing information about
  // peripheral batteries.
  void GetBatteryList(std::vector<base::FilePath>* battery_list);

  // Sends the battery status through D-Bus.
  void SendBatteryStatus(const std::string& path,
                         const std::string& model_name,
                         int level);

  // Asynchronous I/O success and error handlers, respectively.
  void ReadCallback(const std::string& path,
                    const std::string& model_name,
                    const std::string& data);
  void ErrorCallback(const std::string& path,
                     const std::string& model_name);

  DBusSenderInterface* dbus_sender_;  // weak

  // Path containing battery info for peripheral devices.
  base::FilePath peripheral_battery_path_;

  // Calls ReadBatteryStatuses().
  base::OneShotTimer<PeripheralBatteryWatcher> poll_timer_;

  // Time between polls of the peripheral battery reading, in milliseconds.
  int poll_interval_ms_;

  // AsyncFileReaders for different peripheral batteries.
  ScopedVector<AsyncFileReader> battery_readers_;

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryWatcher);
};

}  // namespace system
}  // namespace power_manager


#endif  // POWER_MANAGER_POWERD_SYSTEM_PERIPHERAL_BATTERY_WATCHER_H_
