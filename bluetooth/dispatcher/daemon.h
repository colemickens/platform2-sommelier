// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_DAEMON_H_
#define BLUETOOTH_DISPATCHER_DAEMON_H_

#include <memory>

#include "bluetooth/dispatcher/suspend_manager.h"

namespace bluetooth {

// Main class within btdispatch daemon that ties all other classes together.
class Daemon {
 public:
  explicit Daemon(scoped_refptr<dbus::Bus> bus);
  ~Daemon();

  // Initializes the daemon D-Bus operations.
  void Init();

 private:
  // The suspend/resume handler.
  std::unique_ptr<SuspendManager> suspend_manager_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_DAEMON_H_
