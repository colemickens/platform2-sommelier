// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/common/dbus_daemon.h"

#include <utility>

#include <sysexits.h>

namespace bluetooth {

DBusDaemon::DBusDaemon(std::unique_ptr<BluetoothDaemon> bluetooth_daemon)
    : bluetooth_daemon_(std::move(bluetooth_daemon)) {}

int DBusDaemon::OnInit() {
  int exit_code = brillo::Daemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return EX_UNAVAILABLE;
  }

  VLOG(1) << "D-Bus connection name = " << bus->GetConnectionName();

  if (!bluetooth_daemon_->Init(bus)) {
    LOG(ERROR) << "Failed to initialize daemon";
    return EX_UNAVAILABLE;
  }

  return EX_OK;
}

}  // namespace bluetooth
