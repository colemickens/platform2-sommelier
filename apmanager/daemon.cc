// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/daemon.h"

#include <sysexits.h>

#include <base/logging.h>

#include "apmanager/dbus/dbus_control.h"

namespace apmanager {

// static
const char Daemon::kAPManagerGroupName[] = "apmanager";
const char Daemon::kAPManagerUserName[] = "apmanager";

Daemon::Daemon(const base::Closure& startup_callback)
    : startup_callback_(startup_callback) {
}

int Daemon::OnInit() {
  int return_code = brillo::Daemon::OnInit();
  if (return_code != EX_OK) {
    return return_code;
  }

  // Setup control interface. The control interface will expose
  // our service (Manager) through its RPC interface.
  control_interface_.reset(new DBusControl());
  control_interface_->Init();

  // Signal that we've acquired all resources.
  startup_callback_.Run();

  return EX_OK;
}

void Daemon::OnShutdown(int* return_code) {
  control_interface_->Shutdown();
}

}  // namespace apmanager
