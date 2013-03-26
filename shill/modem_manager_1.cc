// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_manager.h"

#include <base/bind.h>
#include <base/stl_util.h>
#include <ModemManager/ModemManager.h>

#include "shill/error.h"
#include "shill/logging.h"
#include "shill/modem.h"
#include "shill/proxy_factory.h"

using base::Bind;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace shill {

ModemManager1::ModemManager1(const string &service,
                             const string &path,
                             ModemInfo *modem_info)
    : ModemManager(service,
                   path,
                   modem_info),
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
                              const DBusInterfaceToProperties &properties) {
  if (ModemExists(path)) {
    return;
  }
  shared_ptr<Modem1> modem1(new Modem1(owner(),
                                       service(),
                                       path,
                                       modem_info()));
  RecordAddedModem(modem1);
  InitModem1(modem1, properties);
}

void ModemManager1::InitModem1(shared_ptr<Modem1> modem,
                               const DBusInterfaceToProperties &properties) {
  if (modem == NULL) {
    return;
  }
  modem->CreateDeviceMM1(properties);
}

// signal methods
// Also called by OnGetManagedObjectsReply
void ModemManager1::OnInterfacesAddedSignal(
    const ::DBus::Path &object_path,
    const DBusInterfaceToProperties &properties) {
  if (ContainsKey(properties, MM_DBUS_INTERFACE_MODEM)) {
    AddModem1(object_path, properties);
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
