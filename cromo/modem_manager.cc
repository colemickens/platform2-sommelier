// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>

#include "modem_manager_server.h"
#include "modem_manager.h"

using std::string;

ModemManager::ModemManager(ModemManagerServer& server, const char* tag)
    : server_(server),
      vendor_tag_(tag),
  instance_number_(0) {
}

ModemManager::~ModemManager() {
  ClearModemList();
}

bool ModemManager::Initialize() {
  server_.AddModemManager(this);
  return true;
}

void ModemManager::AddModem(DBus::ObjectAdaptor* modem) {
  modems_[modem->path()] = modem;
}

void ModemManager::RemoveModem(DBus::ObjectAdaptor* modem) {
  modems_.erase(modem->path());
}

void ModemManager::ClearModemList() {
  ModemMap::iterator it;

  for (it = modems_.begin(); it != modems_.end(); ++it) {
    delete (*it).second;
  }
  modems_.clear();
}

string ModemManager::MakePath() {
  char instance[32];
  string path(ModemManagerServer::SERVER_PATH);

  snprintf(instance, sizeof(instance), "%d", instance_number_++);
  return path.append("/").append(vendor_tag_).append("/").append(instance);
}

string ModemManager::vendor_tag() const {
  return vendor_tag_;
}
