// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lorgnette/daemon.h"

#include <sysexits.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/run_loop.h>
#include <chromeos/dbus/service_constants.h>

using std::string;
using std::vector;

namespace lorgnette {

// static
const char Daemon::kScanGroupName[] = "scanner";
const char Daemon::kScanUserName[] = "saned";
const int Daemon::kShutdownTimeoutMilliseconds = 20000;

Daemon::Daemon(const base::Closure& startup_callback)
    : DBusServiceDaemon(kManagerServiceName, "/ObjectManager"),
      startup_callback_(startup_callback) {}

int Daemon::OnInit() {
  int return_code = chromeos::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK) {
    return return_code;
  }

  manager_.reset(new Manager(
      base::Bind(&Daemon::PostponeShutdown, base::Unretained(this))));
  manager_->InitDBus(object_manager_.get());

  PostponeShutdown();

  // Signal that we've acquired all resources.
  startup_callback_.Run();
  return EX_OK;
}

void Daemon::OnShutdown(int* return_code) {
  manager_.reset();
  chromeos::DBusServiceDaemon::OnShutdown(return_code);
}

void Daemon::PostponeShutdown() {
  shutdown_callback_.Reset(base::Bind(&chromeos::Daemon::Quit,
                                      base::Unretained(this)));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      shutdown_callback_.callback(),
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMilliseconds));
}

}  // namespace lorgnette
