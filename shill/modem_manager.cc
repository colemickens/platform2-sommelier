// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_manager.h"

#include <base/logging.h>
#include <base/stl_util-inl.h>

#include "shill/modem.h"
#include "shill/modem_manager_proxy.h"
#include "shill/proxy_factory.h"

using std::string;
using std::tr1::shared_ptr;
using std::vector;

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
      manager_(manager),
      glib_(glib) {}

ModemManager::~ModemManager() {
  Stop();
}

void ModemManager::Start() {
  LOG(INFO) << "Start watching modem manager service: " << service_;
  CHECK_EQ(0, watcher_id_);
  // TODO(petkov): Implement DBus name watching through dbus-c++.
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
  proxy_.reset(
      ProxyFactory::factory()->CreateModemManagerProxy(this, path_, owner_));

  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  vector<DBus::Path> devices = proxy_->EnumerateDevices();
  for (vector<DBus::Path>::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    AddModem(*it);
  }
}

void ModemManager::Disconnect() {
  modems_.clear();
  owner_.clear();
  proxy_.reset();
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

void ModemManager::AddModem(const std::string &path) {
  LOG(INFO) << "Add modem: " << path;
  CHECK(!owner_.empty());
  if (ContainsKey(modems_, path)) {
    LOG(INFO) << "Modem already exists; ignored.";
    return;
  }
  shared_ptr<Modem> modem(
      new Modem(owner_, path, control_interface_, dispatcher_, manager_));
  modems_[path] = modem;
  modem->Init();
}

void ModemManager::RemoveModem(const std::string &path) {
  LOG(INFO) << "Remove modem: " << path;
  CHECK(!owner_.empty());
  modems_.erase(path);
}

}  // namespace shill
