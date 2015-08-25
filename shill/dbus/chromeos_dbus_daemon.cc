// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_daemon.h"

#include <sysexits.h>

#include <chromeos/dbus/service_constants.h>

#include "shill/dbus/chromeos_dbus_control.h"
#include "shill/manager.h"

namespace shill {

// TODO(zqiu): ObjectManager is not being exported as of now. To export
// ObjectManager, initialize DBusServiceDaemon with a valid path.
ChromeosDBusDaemon::ChromeosDBusDaemon(const base::Closure& startup_callback,
                                       const Settings& settings,
                                       Config* config)
    : DBusServiceDaemon(kFlimflamServiceName, ""),
      ChromeosDaemon(settings, config),
      startup_callback_(startup_callback) {}

void ChromeosDBusDaemon::RunMessageLoop() {
  DBusServiceDaemon::Run();
}

int ChromeosDBusDaemon::OnInit() {
  // Manager DBus interface will get registered as part of this init call.
  int return_code = chromeos::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK) {
    return return_code;
  }

  // Signal that we've acquired all resources.
  startup_callback_.Run();

  return EX_OK;
}

void ChromeosDBusDaemon::OnShutdown(int* return_code) {
  ChromeosDaemon::Quit(
      base::Bind(&ChromeosDBusDaemon::OnTerminationCompleted,
                 base::Unretained(this)));

  // Run a message loop to allow shill to complete its termination procedures.
  // This is different from the secondary loop in chromeos::Daemon. This loop
  // will run until we explicitly breakout of the loop, whereas the secondary
  // loop in chromeos::Daemon will run until no more tasks are posted on the
  // loop.  This allows asynchronous D-Bus method calls to complete before
  // exiting.
  chromeos::MessageLoop::current()->Run();

  chromeos::DBusServiceDaemon::OnShutdown(return_code);
}

void ChromeosDBusDaemon::RegisterDBusObjectsAsync(
    chromeos::dbus_utils::AsyncEventSequencer* sequencer) {
  // Put the Init call here instead of the constructor so that
  // ChromeosDBusControl initialization is performed after the |bus_| is
  // initialized.
  // |bus_| is initialized in chromeos::DBusServiceDaemon::OnInit()
  Init(new ChromeosDBusControl(bus_, &dispatcher_), &dispatcher_);

  // Register "org.chromium.flimflam.Manager" interface.
  // The daemon will request the ownership of the DBus service
  // "org.chromium.flimflam" after Manager interface registration is
  // completed.
  manager()->RegisterAsync(
      base::Bind(
          &ChromeosDBusDaemon::OnDBusServiceRegistered,
          base::Unretained(this),
          sequencer->GetHandler("Manager.RegisterAsync() failed.", true)));
}

void ChromeosDBusDaemon::OnDBusServiceRegistered(
    const base::Callback<void(bool)>& completion_action, bool success) {
  // The daemon will take over the ownership of the DBus service in this
  // callback.  The daemon will crash if registration failed.
  completion_action.Run(success);

  // We can start the manager now that we have ownership of the D-Bus service.
  // Doing so earlier would allow the manager to emit signals before service
  // ownership was acquired.
  ChromeosDaemon::Start();
}

void ChromeosDBusDaemon::OnTerminationCompleted() {
  // Break out of the termination loop, to continue on with other shutdown
  // tasks.
  chromeos::MessageLoop::current()->BreakLoop();
}

}  // namespace shill
