// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_DISPATCHER_DAEMON_H_
#define BLUETOOTH_DISPATCHER_DISPATCHER_DAEMON_H_

#include <memory>

#include "bluetooth/common/bluetooth_daemon.h"
#include "bluetooth/dispatcher/dispatcher.h"
#include "bluetooth/dispatcher/suspend_manager.h"

namespace bluetooth {

// Main class within btdispatch daemon that ties all other classes together.
class DispatcherDaemon : public BluetoothDaemon {
 public:
  DispatcherDaemon() = default;
  ~DispatcherDaemon() = default;

  // Initializes the daemon D-Bus operations.
  bool Init(scoped_refptr<dbus::Bus> bus) override;

 private:
  // The suspend/resume handler for pausing/unpausing discovery during system
  // suspend.
  std::unique_ptr<SuspendManager> suspend_manager_;

  // Exposes BlueZ-compatible D-Bus API and handles the client requests.
  std::unique_ptr<Dispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherDaemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_DISPATCHER_DAEMON_H_
