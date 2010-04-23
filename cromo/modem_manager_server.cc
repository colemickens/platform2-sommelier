// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "modem_manager_server.h"
#include "modem_manager.h"
#include "plugin_manager.h"

using std::vector;
using std::cout;
using std::endl;

const char* ModemManagerServer::SERVER_NAME = "org.chromium.ModemManager";
const char* ModemManagerServer::SERVER_PATH = "/org/chromium/ModemManager";

ModemManagerServer::ModemManagerServer(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, SERVER_PATH) {
}

ModemManagerServer::~ModemManagerServer() {
  vector<ModemManager *>::iterator it;

  for (it = modem_managers_.begin(); it != modem_managers_.end(); it++) {
    delete *it;
  }
  modem_managers_.clear();
}

vector<DBus::Path> ModemManagerServer::EnumerateDevices() {
  vector<DBus::Path > allpaths;
  vector<ModemManager *>::iterator it;

  for (it = modem_managers_.begin(); it != modem_managers_.end(); it++) {
    vector<DBus::Path> paths = (*it)->EnumerateDevices();
    allpaths.insert(allpaths.end(), paths.begin(), paths.end());
  }
  return allpaths;
}

void ModemManagerServer::AddModemManager(ModemManager *manager) {
  cout << "AddModemManager(" << manager->vendor_tag() << ")" << endl;
  modem_managers_.push_back(manager);
}
