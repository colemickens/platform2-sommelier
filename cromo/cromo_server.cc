// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo_server.h"

#include <glog/logging.h>

#include "modem_handler.h"
#include "plugin_manager.h"

using std::vector;

const char* CromoServer::kServiceName = "org.chromium.ModemManager";
const char* CromoServer::kServicePath = "/org/chromium/ModemManager";

CromoServer::CromoServer(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, kServicePath) {
}

CromoServer::~CromoServer() {
  for (vector<ModemHandler*>::iterator it = modem_handlers_.begin();
       it != modem_handlers_.end(); it++) {
    delete *it;
  }
  modem_handlers_.clear();
}

vector<DBus::Path> CromoServer::EnumerateDevices(DBus::Error& error) {
  vector<DBus::Path> allpaths;

  for (vector<ModemHandler*>::iterator it = modem_handlers_.begin();
       it != modem_handlers_.end(); it++) {
    vector<DBus::Path> paths = (*it)->EnumerateDevices(error);
    allpaths.insert(allpaths.end(), paths.begin(), paths.end());
  }
  return allpaths;
}

void CromoServer::AddModemHandler(ModemHandler* handler) {
  LOG(INFO) << "AddModemHandler(" << handler->vendor_tag() << ")";
  modem_handlers_.push_back(handler);
}
