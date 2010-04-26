// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dummy_modem_manager.h"

#include <iostream>

#include "dummy_modem.h"
#include "modem_manager_server.h"
#include "plugin.h"

using std::vector;
using std::cout;
using std::cerr;
using std::endl;

DummyModemManager::DummyModemManager(ModemManagerServer& server)
    : ModemManager(server, "dummy") {
}

bool
DummyModemManager::Initialize() {
  // ... first do any modem-manager-specific initialization ...

  return ModemManager::Initialize();
}

// Enumerate the existing devices, and add them to the list of devices
// that are managed by the ChromeOS modem manager

vector<DBus::Path> DummyModemManager::EnumerateDevices() {
  vector<DBus::Path> paths;

  DummyModem* dummy = new DummyModem(server().conn(), MakePath());
  paths.push_back(dummy->path());
  AddModem(dummy);
  return paths;
}

static void onload(ModemManagerServer* server) {
  cout << __FILE__ << ": onload() called" << endl;
  ModemManager* mm = new DummyModemManager(*server);
  if (!mm->Initialize())
    cerr << "Failed to initialize DummyModemManager" << endl;
}

static void onunload() {
}

CROMO_DEFINE_PLUGIN(dummy, onload, onunload)
