// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/dummy_modem_handler.h"

#include <base/logging.h>

#include "cromo/cromo_server.h"
#include "cromo/dummy_modem.h"
#include "cromo/plugin.h"

using std::vector;

DummyModemHandler::DummyModemHandler(CromoServer& server)  // NOLINT - refs.
    : ModemHandler(server, "dummy") {
}

bool DummyModemHandler::Initialize() {
  // ... do any modem-manager-specific initialization ...
  RegisterSelf();
  return true;
}

// Enumerate the existing devices, and add them to the list of devices
// that are managed by the ChromeOS modem manager

vector<DBus::Path> DummyModemHandler::EnumerateDevices(
    DBus::Error& error) {  // NOLINT(runtime/references)
  vector<DBus::Path> paths;

  DummyModem* dummy = new DummyModem(server().conn(), MakePath());
  paths.push_back(dummy->path());
  return paths;
}

void onload(CromoServer* server) {
  LOG(INFO) << __FILE__ << ": onload() called";
  ModemHandler* mh = new DummyModemHandler(*server);
  if (!mh->Initialize())
    LOG(ERROR) << "Failed to initialize DummyModemHandler";
}

static void onunload() {
}

CROMO_DEFINE_PLUGIN(dummy, onload, onunload)
