// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/dbus_daemon.h"

#include <utility>

#include <sysexits.h>

namespace bluetooth {

DBusDaemon::DBusDaemon(std::unique_ptr<BluetoothDaemon> bluetooth_daemon)
    : bluetooth_daemon_(std::move(bluetooth_daemon)) {}

int DBusDaemon::OnInit() {
  brillo::Daemon::OnInit();

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return EX_UNAVAILABLE;
  }

  VLOG(1) << "D-Bus connection name = " << bus->GetConnectionName();

  bluetooth_daemon_->Init(bus);

  return EX_OK;
}

}  // namespace bluetooth
