// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_COMMON_BLUETOOTH_DAEMON_H_
#define BLUETOOTH_COMMON_BLUETOOTH_DAEMON_H_

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

namespace bluetooth {

// The interface of bluetooth::DBusDaemon's delegate.
class BluetoothDaemon {
 public:
  BluetoothDaemon() = default;
  virtual ~BluetoothDaemon() = default;

  // Initializes the daemon's D-Bus operation.
  virtual void Init(scoped_refptr<dbus::Bus> bus) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDaemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_COMMON_BLUETOOTH_DAEMON_H_
