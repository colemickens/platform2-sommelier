// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dummy_modem_handler.h"

#include <iostream>

#include "dummy_modem.h"
#include "cromo_server.h"
#include "plugin.h"

using std::vector;
using std::cout;
using std::cerr;
using std::endl;

DummyModemHandler::DummyModemHandler(CromoServer& server)
    : ModemHandler(server, "dummy") {
}

bool
DummyModemHandler::Initialize() {
  // ... do any modem-manager-specific initialization ...
  RegisterSelf();
  return true;
}

// Enumerate the existing devices, and add them to the list of devices
// that are managed by the ChromeOS modem manager

vector<DBus::Path> DummyModemHandler::EnumerateDevices(DBus::Error& error) {
  vector<DBus::Path> paths;

  DummyModem* dummy = new DummyModem(server().conn(), MakePath());
  paths.push_back(dummy->path());
  return paths;
}

void onload(CromoServer* server) {
  cout << __FILE__ << ": onload() called" << endl;
  ModemHandler* mh = new DummyModemHandler(*server);
  if (!mh->Initialize())
    cerr << "Failed to initialize DummyModemHandler" << endl;
}

static void onunload() {
}

CROMO_DEFINE_PLUGIN(dummy, onload, onunload)
