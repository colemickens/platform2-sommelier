// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps daemon. It handles calls from multiple processes via D-Bus.
//

#include <signal.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <chromeos/syslog_logging.h>
#include <dbus-c++/dbus.h>

#include "chaps/chaps_adaptor.h"
#include "chaps/chaps_service_redirect.h"

scoped_ptr<DBus::BusDispatcher> g_dispatcher;

static void Terminate(int sig) {
  g_dispatcher->leave();
}

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine::StringVector args = CommandLine::ForCurrentProcess()->args();
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  g_dispatcher.reset(new DBus::BusDispatcher());
  CHECK(g_dispatcher.get());
  DBus::default_dispatcher = g_dispatcher.get();
  if (args.size() == 0)
    // Currently, only redirecting to a library is implemented.
    LOG(FATAL) << "Usage: chapsd LIBRARY";
  LOG(INFO) << "Starting PKCS #11 services (" << args[0] << ").";
  scoped_ptr<chaps::ChapsServiceRedirect> lib_target(
      new chaps::ChapsServiceRedirect(args[0].c_str()));
  CHECK(lib_target.get()) << "Out of memory.";
  if (!lib_target->Init())
    LOG(FATAL) << "Failed to initialize PKCS #11 library (" << args[0] << ").";
  signal(SIGTERM, Terminate);
  signal(SIGINT, Terminate);
  try {
    chaps::ChapsAdaptor adaptor(lib_target.get());
    g_dispatcher->enter();
  } catch (DBus::Error err) {
    LOG(FATAL) << "DBus::Error - " << err.what();
  }
  return 0;
}
