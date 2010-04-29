// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modem_handler.h"

#include <cstdio>

#include "cromo_server.h"

using std::string;

ModemHandler::ModemHandler(CromoServer& server, const string& tag)
    : server_(server),
      vendor_tag_(tag),
      instance_number_(0) {
}

ModemHandler::~ModemHandler() {
  ClearModemList();
}

void ModemHandler::RegisterSelf() {
  server_.AddModemHandler(this);
}

void ModemHandler::AddModem(DBus::ObjectAdaptor* modem) {
  modems_[modem->path()] = modem;
}

void ModemHandler::RemoveModem(DBus::ObjectAdaptor* modem) {
  modems_.erase(modem->path());
}

void ModemHandler::ClearModemList() {
  for (ModemMap::iterator it = modems_.begin(); it != modems_.end(); ++it) {
    delete (*it).second;
  }
  modems_.clear();
}

string ModemHandler::MakePath() {
  char instance[32];
  string path(CromoServer::kServicePath);

  snprintf(instance, sizeof(instance), "%d", instance_number_++);
  return path.append("/").append(vendor_tag_).append("/").append(instance);
}
