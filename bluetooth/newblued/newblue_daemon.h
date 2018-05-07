// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

#include "bluetooth/newblued/bluetooth_daemon.h"

namespace bluetooth {

class NewblueDaemon : public BluetoothDaemon {
 public:
  NewblueDaemon() = default;
  ~NewblueDaemon() = default;

  // Initializes the daemon D-Bus operations.
  void Init(scoped_refptr<dbus::Bus> bus) override;

 private:
  scoped_refptr<dbus::Bus> bus_;

  DISALLOW_COPY_AND_ASSIGN(NewblueDaemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_
