// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/daemons/daemon.h>

#include <sysexits.h>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/run_loop.h>

namespace chromeos {

Daemon::Daemon() {
}

Daemon::~Daemon() {
}

int Daemon::Run() {
  int return_code = OnInit();
  if (return_code != EX_OK) {
    quit_closure_.Reset();
    return return_code;
  }

  // Scope.
  {
    // The main run loop.
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    quit_closure_.Reset();
  }

  OnShutdown(&return_code);

  // Scope.
  {
    // base::RunLoop::QuitClosure() causes the message loop to quit
    // immediately, even if pending tasks are still queued.
    // Run a secondary loop to make sure all those are processed.
    // This becomes important when working with D-Bus since dbus::Bus does
    // a bunch of clean-up tasks asynchronously when shutting down.
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  return return_code;
}

void Daemon::Quit() {
  message_loop_.PostTask(FROM_HERE, quit_closure_);
}

int Daemon::OnInit() {
  async_signal_handler_.Init();
  for (int signal : {SIGTERM, SIGINT}) {
    async_signal_handler_.RegisterHandler(
        signal, base::Bind(&Daemon::Shutdown, base::Unretained(this)));
  }
  async_signal_handler_.RegisterHandler(
      SIGHUP, base::Bind(&Daemon::Restart, base::Unretained(this)));
  return EX_OK;
}

void Daemon::OnShutdown(int* return_code) {
  // Do nothing.
}

bool Daemon::OnRestart() {
  // Not handled.
  return false;  // Returning false will shut down the daemon instead.
}

bool Daemon::Shutdown(const signalfd_siginfo& info) {
  Quit();
  return true;  // Unregister the signal handler.
}

bool Daemon::Restart(const signalfd_siginfo& info) {
  if (OnRestart())
    return false;  // Keep listening to the signal.
  Quit();
  return true;  // Unregister the signal handler.
}

}  // namespace chromeos
