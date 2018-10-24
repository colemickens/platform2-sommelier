// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/dbus_control.h"

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include <chromeos/dbus/service_constants.h>

namespace wimax_manager {

namespace {

base::LazyInstance<DBusControl>::DestructorAtExit g_dbus_control =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DBusControl::DBusControl() {
  Initialize();
}

DBusControl::~DBusControl() {
  Finalize();
}

// static
DBusControl* DBusControl::GetInstance() {
  return g_dbus_control.Pointer();
}

// static
DBus::Connection* DBusControl::GetConnection() {
  return GetInstance()->connection_.get();
}

void DBusControl::Initialize() {
  bus_dispatcher_.reset(new DBus::Glib::BusDispatcher());
  DBus::default_dispatcher = bus_dispatcher_.get();
  bus_dispatcher_->attach(nullptr);

  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
  CHECK(connection_->acquire_name(kWiMaxManagerServiceName))
      << "Failed to acquire D-Bus name " << kWiMaxManagerServiceName;
}

void DBusControl::Finalize() {
  connection_.reset();
  DBus::default_dispatcher = nullptr;
  bus_dispatcher_.reset();
}

}  // namespace wimax_manager
