// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_manager.h"

#include <base/logging.h>
#include <base/stl_util.h>

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
                           Metrics *metrics,
                           Manager *manager,
                           GLib *glib,
                           mobile_provider_db *provider_db)
    : proxy_factory_(ProxyFactory::GetInstance()),
      service_(service),
      path_(path),
      watcher_id_(0),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      glib_(glib),
      provider_db_(provider_db) {}

ModemManager::~ModemManager() {
  Stop();
}

void ModemManager::Start() {
  LOG(INFO) << "Start watching modem manager service: " << service_;
  CHECK_EQ(0U, watcher_id_);
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
  // Inheriting classes call this superclass method.
  owner_ = owner;
}

void ModemManager::Disconnect() {
  // Inheriting classes call this superclass method.
  modems_.clear();
  owner_.clear();
}

void ModemManager::OnAppear(GDBusConnection */*connection*/,
                            const gchar *name,
                            const gchar *name_owner,
                            gpointer user_data) {
  LOG(INFO) << "Modem manager " << name << " appeared. Owner: " << name_owner;
  ModemManager *manager = reinterpret_cast<ModemManager *>(user_data);
  manager->Connect(name_owner);
}

void ModemManager::OnVanish(GDBusConnection */*connection*/,
                            const gchar *name,
                            gpointer user_data) {
  LOG(INFO) << "Modem manager " << name << " vanished.";
  ModemManager *manager = reinterpret_cast<ModemManager *>(user_data);
  manager->Disconnect();
}

void ModemManager::AddModem(const string &path) {
  LOG(INFO) << "Add modem: " << path;
  CHECK(!owner_.empty());
  if (ContainsKey(modems_, path)) {
    LOG(INFO) << "Modem already exists; ignored.";
    return;
  }
  shared_ptr<Modem> modem(new Modem(owner_,
                                    path,
                                    control_interface_,
                                    dispatcher_,
                                    metrics_,
                                    manager_,
                                    provider_db_));
  modems_[path] = modem;
  InitModem(modem);
}

void ModemManager::RemoveModem(const string &path) {
  LOG(INFO) << "Remove modem: " << path;
  CHECK(!owner_.empty());
  modems_.erase(path);
}

void ModemManager::OnDeviceInfoAvailable(const string &link_name) {
  for (Modems::const_iterator it = modems_.begin(); it != modems_.end(); ++it) {
    it->second->OnDeviceInfoAvailable(link_name);
  }
}

// ModemManagerClassic
ModemManagerClassic::ModemManagerClassic(const std::string &service,
                                         const std::string &path,
                                         ControlInterface *control_interface,
                                         EventDispatcher *dispatcher,
                                         Metrics *metrics,
                                         Manager *manager,
                                         GLib *glib,
                                         mobile_provider_db *provider_db) :
    ModemManager(service,
                 path,
                 control_interface,
                 dispatcher,
                 metrics,
                 manager,
                 glib,
                 provider_db) {}

ModemManagerClassic::~ModemManagerClassic() {}

void ModemManagerClassic::Connect(const string &supplied_owner) {
  ModemManager::Connect(supplied_owner);
  proxy_.reset(proxy_factory()->CreateModemManagerProxy(this, path(), owner()));

  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  vector<DBus::Path> devices = proxy_->EnumerateDevices();
  for (vector<DBus::Path>::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    AddModem(*it);
  }
}

void ModemManagerClassic::Disconnect() {
  ModemManager::Disconnect();
  proxy_.reset();
}

void ModemManagerClassic::InitModem(shared_ptr<Modem> modem) {
  modem->Init();
}

}  // namespace shill
