// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_manager.h"

#include <algorithm>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <mm/mm-modem.h>
#include <mm/ModemManager-names.h>

#include "shill/error.h"
#include "shill/modem.h"
#include "shill/modem_manager_proxy.h"
#include "shill/proxy_factory.h"

using base::Bind;
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
ModemManagerClassic::ModemManagerClassic(const string &service,
                                         const string &path,
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
    AddModemClassic(*it);
  }
}

void ModemManagerClassic::AddModemClassic(const string &path) {
  if (ModemExists(path)) {
    return;
  }
  shared_ptr<ModemClassic> modem(new ModemClassic(owner(),
                                                  path,
                                                  control_interface(),
                                                  dispatcher(),
                                                  metrics(),
                                                  manager(),
                                                  provider_db()));
  RecordAddedModem(modem);
  InitModemClassic(modem);
}

void ModemManagerClassic::Disconnect() {
  ModemManager::Disconnect();
  proxy_.reset();
}

void ModemManagerClassic::InitModemClassic(shared_ptr<ModemClassic> modem) {
  // TODO(rochberg): Switch to asynchronous calls (crosbug.com/17583).
  if (modem == NULL) {
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

ModemManager1::ModemManager1(const string &service,
                             const string &path,
                             ControlInterface *control_interface,
                             EventDispatcher *dispatcher,
                             Metrics *metrics,
                             Manager *manager,
                             GLib *glib,
                             mobile_provider_db *provider_db)
    : ModemManager(service,
                   path,
                   control_interface,
                   dispatcher,
                   metrics,
                   manager,
                   glib,
                   provider_db),
      weak_ptr_factory_(this) {}

ModemManager1::~ModemManager1() {}

void ModemManager1::Connect(const string &supplied_owner) {
  ModemManager::Connect(supplied_owner);
  proxy_.reset(
      proxy_factory()->CreateDBusObjectManagerProxy(path(), owner()));
  proxy_->set_interfaces_added_callback(
      Bind(&ModemManager1::OnInterfacesAddedSignal,
           weak_ptr_factory_.GetWeakPtr()));
  proxy_->set_interfaces_removed_callback(
      Bind(&ModemManager1::OnInterfacesRemovedSignal,
           weak_ptr_factory_.GetWeakPtr()));

  // TODO(rochberg):  Make global kDBusDefaultTimeout and use it here
  Error error;
  proxy_->GetManagedObjects(&error,
                            Bind(&ModemManager1::OnGetManagedObjectsReply,
                                 weak_ptr_factory_.GetWeakPtr()),
                            5000);
}

void ModemManager1::Disconnect() {
  ModemManager::Disconnect();
  proxy_.reset();
}

void ModemManager1::AddModem1(const string &path,
                              const DBusInterfaceToProperties &i_to_p) {
  if (ModemExists(path)) {
    return;
  }
  shared_ptr<Modem1> modem1(new Modem1(owner(),
                                       path,
                                       control_interface(),
                                       dispatcher(),
                                       metrics(),
                                       manager(),
                                       provider_db()));
  RecordAddedModem(modem1);
  InitModem1(modem1, i_to_p);
}

void ModemManager1::InitModem1(shared_ptr<Modem1> modem,
                               const DBusInterfaceToProperties &i_to_p) {
  if (modem == NULL) {
    return;
  }
  modem->CreateDeviceMM1(i_to_p);
}

// signal methods
// Also called by OnGetManagedObjectsReply
void ModemManager1::OnInterfacesAddedSignal(
    const ::DBus::Path &object_path,
    const DBusInterfaceToProperties &interface_to_properties) {
  if (ContainsKey(interface_to_properties, MM_DBUS_INTERFACE_MODEM)) {
    AddModem1(object_path, interface_to_properties);
  } else {
    LOG(ERROR) << "Interfaces added, but not modem interface.";
  }
}

void ModemManager1::OnInterfacesRemovedSignal(
    const ::DBus::Path &object_path,
    const vector<string> &interfaces) {
  LOG(INFO) << "MM1:  Removing interfaces from " << object_path;
  if (find(interfaces.begin(),
           interfaces.end(),
           MM_DBUS_INTERFACE_MODEM) != interfaces.end()) {
    RemoveModem(object_path);
  } else {
    // In theory, a modem could drop, say, 3GPP, but not CDMA.  In
    // practice, we don't expect this
    LOG(ERROR) << "Interfaces removed, but not modem interface";
  }
}

// DBusObjectManagerProxy async method call
void ModemManager1::OnGetManagedObjectsReply(
    const DBusObjectsWithProperties &objects,
    const Error &error) {
  if (error.IsSuccess()) {
    DBusObjectsWithProperties::const_iterator m;
    for (m = objects.begin(); m != objects.end(); ++m) {
      OnInterfacesAddedSignal(m->first, m->second);
    }
  }
}

}  // namespace shill
