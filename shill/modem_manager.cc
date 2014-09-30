// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_manager.h"

#include <base/stl_util.h>
#include <mm/mm-modem.h>

#include "shill/dbus_manager.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/modem.h"
#include "shill/modem_manager_proxy.h"
#include "shill/proxy_factory.h"

using std::string;
using std::shared_ptr;
using std::vector;

namespace shill {

ModemManager::ModemManager(const string &service,
                           const string &path,
                           ModemInfo *modem_info)
    : proxy_factory_(ProxyFactory::GetInstance()),
      service_(service),
      path_(path),
      modem_info_(modem_info) {}

ModemManager::~ModemManager() {
  Stop();
}

void ModemManager::Start() {
  LOG(INFO) << "Start watching modem manager service: " << service_;
  CHECK(!name_watcher_);
  name_watcher_.reset(modem_info_->manager()->dbus_manager()->CreateNameWatcher(
      service_,
      base::Bind(&ModemManager::OnAppear, base::Unretained(this)),
      base::Bind(&ModemManager::OnVanish, base::Unretained(this))));
}

void ModemManager::Stop() {
  LOG(INFO) << "Stop watching modem manager service: " << service_;
  name_watcher_.reset();
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

void ModemManager::OnAppear(const string &name, const string &owner) {
  LOG(INFO) << "Modem manager " << name << " appeared. Owner: " << owner;
  Connect(owner);
}

void ModemManager::OnVanish(const string &name) {
  LOG(INFO) << "Modem manager " << name << " vanished.";
  Disconnect();
}

bool ModemManager::ModemExists(const std::string &path) const {
  CHECK(!owner_.empty());
  if (ContainsKey(modems_, path)) {
    LOG(INFO) << "ModemExists: " << path << " already exists.";
    return true;
  } else {
    return false;
  }
}

void ModemManager::RecordAddedModem(shared_ptr<Modem> modem) {
  modems_[modem->path()] = modem;
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
ModemManagerClassic::ModemManagerClassic(
    const string &service,
    const string &path,
    ModemInfo *modem_info)
    : ModemManager(service,
                   path,
                   modem_info) {}

ModemManagerClassic::~ModemManagerClassic() {}

void ModemManagerClassic::Connect(const string &supplied_owner) {
  ModemManager::Connect(supplied_owner);
  proxy_.reset(proxy_factory()->CreateModemManagerProxy(this, path(), owner()));
  // TODO(petkov): Switch to asynchronous calls (crbug.com/200687).
  vector<DBus::Path> devices = proxy_->EnumerateDevices();

  for (vector<DBus::Path>::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    AddModemClassic(*it);
  }
}

void ModemManagerClassic::AddModemClassic(const string &path) {
  if (ModemExists(path)) {
    return;
  }
  shared_ptr<ModemClassic> modem(new ModemClassic(owner(),
                                                  service(),
                                                  path,
                                                  modem_info()));
  RecordAddedModem(modem);
  InitModemClassic(modem);
}

void ModemManagerClassic::Disconnect() {
  ModemManager::Disconnect();
  proxy_.reset();
}

void ModemManagerClassic::InitModemClassic(shared_ptr<ModemClassic> modem) {
  // TODO(rochberg): Switch to asynchronous calls (crbug.com/200687).
  if (modem == nullptr) {
    return;
  }

  scoped_ptr<DBusPropertiesProxyInterface> properties_proxy(
      proxy_factory()->CreateDBusPropertiesProxy(modem->path(),
                                                 modem->owner()));
  DBusPropertiesMap properties =
      properties_proxy->GetAll(MM_MODEM_INTERFACE);

  modem->CreateDeviceClassic(properties);
}

void ModemManagerClassic::OnDeviceAdded(const string &path) {
  AddModemClassic(path);
}

void ModemManagerClassic::OnDeviceRemoved(const string &path) {
  RemoveModem(path);
}

}  // namespace shill
