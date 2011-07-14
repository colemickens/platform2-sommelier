// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_manager.h"

#include <base/logging.h>

using std::string;

namespace shill {

ModemManager::ModemManager(const string &service,
                           const string &path,
                           ControlInterface *control_interface,
                           EventDispatcher *dispatcher,
                           Manager *manager,
                           GLib *glib)
    : service_(service),
      path_(path),
      watcher_id_(0),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      glib_(glib) {}

ModemManager::~ModemManager() {
  Stop();
}

void ModemManager::Start() {
  LOG(INFO) << "Start watching modem manager service: " << service_;
  CHECK_EQ(0, watcher_id_);
  watcher_id_ = glib_->BusWatchName(G_BUS_TYPE_SYSTEM,
                                    service_.c_str(),
                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                    OnAppear,
                                    OnVanish,
                                    this,
                                    NULL);
}

void ModemManager::Stop() {
  LOG(INFO) << "Stop watching modem manager service: " << service_;
  if (watcher_id_) {
    glib_->BusUnwatchName(watcher_id_);
    watcher_id_ = 0;
  }
  Disconnect();
}

void ModemManager::Connect(const string &owner) {
  owner_ = owner;
  // TODO(petkov): Create a proxy to the manager and enumerate its devices.
}

void ModemManager::Disconnect() {
  owner_.clear();
  // TODO(petkov): Destroy manager's proxy and its devices.
}

void ModemManager::OnAppear(GDBusConnection *connection,
                            const gchar *name,
                            const gchar *name_owner,
                            gpointer user_data) {
  LOG(INFO) << "Modem manager " << name << " appeared. Owner: " << name_owner;
  ModemManager *manager = reinterpret_cast<ModemManager *>(user_data);
  manager->Connect(name_owner);
}

void ModemManager::OnVanish(GDBusConnection *connection,
                            const gchar *name,
                            gpointer user_data) {
  LOG(INFO) << "Modem manager " << name << " vanished.";
  ModemManager *manager = reinterpret_cast<ModemManager *>(user_data);
  manager->Disconnect();
}

}  // namespace shill
