// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/daemon.h"

#include <sysexits.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/run_loop.h>
#include <chromeos/dbus/service_constants.h>

namespace apmanager {

// static
const char Daemon::kAPManagerGroupName[] = "apmanager";
const char Daemon::kAPManagerUserName[] = "apmanager";

Daemon::Daemon(const base::Closure& startup_callback)
    : DBusServiceDaemon(kServiceName, "/"),
      startup_callback_(startup_callback) {}

int Daemon::OnInit() {
  int return_code = chromeos::DBusServiceDaemon::OnInit();
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
  chromeos::DBusServiceDaemon::OnShutdown(return_code);
}

void Daemon::RegisterDBusObjectsAsync(
    chromeos::dbus_utils::AsyncEventSequencer* sequencer) {
  manager_.reset(new apmanager::Manager());
  manager_->RegisterAsync(object_manager_.get(), sequencer);
}

}  // namespace apmanager
