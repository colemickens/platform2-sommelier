// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/daemon.h"

#include <sysexits.h>

#include <base/logging.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/run_loop.h>

#include "apmanager/dbus/dbus_control.h"

namespace apmanager {

namespace {
const char kAPManagerServiceName[] = "org.chromium.apmanager";
const char kAPMRootServicePath[] = "/org/chromium/apmanager";
}  // namespace

// static
#if !defined(__ANDROID__)
const char Daemon::kAPManagerGroupName[] = "apmanager";
const char Daemon::kAPManagerUserName[] = "apmanager";
#else
const char Daemon::kAPManagerGroupName[] = "system";
const char Daemon::kAPManagerUserName[] = "system";
#endif  // __ANDROID__

Daemon::Daemon(const base::Closure& startup_callback)
    : DBusServiceDaemon(kAPManagerServiceName, kAPMRootServicePath),
      startup_callback_(startup_callback) {
}

int Daemon::OnInit() {
  int return_code = brillo::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK) {
    return return_code;
  }

  // Signal that we've acquired all resources.
  startup_callback_.Run();

  // Start manager.
  manager_->Start();

  return EX_OK;
}

void Daemon::OnShutdown(int* return_code) {
  manager_.reset();
  brillo::DBusServiceDaemon::OnShutdown(return_code);
}

void Daemon::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  control_interface_.reset(new DBusControl(bus_));
  manager_.reset(new apmanager::Manager());
  // TODO(zqiu): remove object_manager_ and bus_ references once we moved over
  // to use control interface for creating adaptors.
  manager_->RegisterAsync(control_interface_.get(),
                          object_manager_.get(),
                          bus_,
                          sequencer);
}

}  // namespace apmanager
