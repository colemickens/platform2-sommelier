// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_info.h"

#include <mm/mm-modem.h>

#include "shill/modem_manager.h"

using std::string;

namespace shill {

const char ModemInfo::kCromoService[] = "org.chromium.ModemManager";
const char ModemInfo::kCromoPath[] = "/org/chromium/ModemManager";

ModemInfo::ModemInfo(ControlInterface *control_interface,
                     EventDispatcher *dispatcher,
                     Manager *manager,
                     GLib *glib)
    : control_interface_(control_interface),
      dispatcher_(dispatcher_),
      manager_(manager),
      glib_(glib) {}

ModemInfo::~ModemInfo() {
  Stop();
}

void ModemInfo::Start() {
  RegisterModemManager(MM_MODEMMANAGER_SERVICE, MM_MODEMMANAGER_PATH);
  RegisterModemManager(kCromoService, kCromoPath);
}

void ModemInfo::Stop() {
  modem_managers_.reset();
}

void ModemInfo::RegisterModemManager(const string &service,
                                     const string &path) {
  ModemManager *manager = new ModemManager(service,
                                           path,
                                           control_interface_,
                                           dispatcher_,
                                           manager_,
                                           glib_);
  modem_managers_.push_back(manager);  // Passes ownership.
  manager->Start();
}

}  // namespace shill
