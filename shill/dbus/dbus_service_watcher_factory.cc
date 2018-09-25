// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/dbus_service_watcher_factory.h"

#include <memory>

#include "shill/dbus/chromeos_dbus_service_watcher.h"

namespace shill {

namespace {
base::LazyInstance<DBusServiceWatcherFactory>::Leaky
    g_dbus_service_watcher_factory = LAZY_INSTANCE_INITIALIZER;
}  // namespace

DBusServiceWatcherFactory::DBusServiceWatcherFactory() {}
DBusServiceWatcherFactory::~DBusServiceWatcherFactory() {}

DBusServiceWatcherFactory* DBusServiceWatcherFactory::GetInstance() {
  return g_dbus_service_watcher_factory.Pointer();
}

std::unique_ptr<ChromeosDBusServiceWatcher>
DBusServiceWatcherFactory::CreateDBusServiceWatcher(
    scoped_refptr<dbus::Bus> bus,
    const std::string& connection_name,
    const base::Closure& on_connection_vanish) {
  return std::make_unique<ChromeosDBusServiceWatcher>(
      bus, connection_name, on_connection_vanish);
}

}  // namespace shill
