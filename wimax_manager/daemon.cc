// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/daemon.h"

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include <base/logging.h>

#include "wimax_manager/manager.h"

namespace wimax_manager {

namespace {

const char kWiMaxManagerServiceName[] = "org.chromium.WiMaxManager";

}  // namespace

Daemon::Daemon() {
}

Daemon::~Daemon() {
  Finalize();
}

bool Daemon::Initialize() {
  dbus_dispatcher_.reset(new(std::nothrow) DBus::Glib::BusDispatcher());
  CHECK(dbus_dispatcher_.get());
  DBus::default_dispatcher = dbus_dispatcher_.get();
  dbus_dispatcher_->attach(NULL);

  dbus_connection_.reset(
      new(std::nothrow) DBus::Connection(DBus::Connection::SystemBus()));
  CHECK(dbus_connection_.get());
  dbus_connection_->request_name(kWiMaxManagerServiceName);

  manager_.reset(new(std::nothrow) Manager(dbus_connection_.get()));
  CHECK(manager_.get());
  return manager_->Initialize();
}

bool Daemon::Finalize() {
  manager_.reset();
  dbus_connection_.reset();
  DBus::default_dispatcher = NULL;
  dbus_dispatcher_.reset();
  return true;
}

}  // namespace wimax_manager
