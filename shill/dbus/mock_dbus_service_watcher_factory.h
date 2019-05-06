// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_MOCK_DBUS_SERVICE_WATCHER_FACTORY_H_
#define SHILL_DBUS_MOCK_DBUS_SERVICE_WATCHER_FACTORY_H_

#include <memory>
#include <string>

#include <gmock/gmock.h>

#include "shill/dbus/dbus_service_watcher_factory.h"

namespace shill {

class MockDBusServiceWatcherFactory : public DBusServiceWatcherFactory {
 public:
  MockDBusServiceWatcherFactory() = default;
  virtual ~MockDBusServiceWatcherFactory() = default;

  MOCK_METHOD3(CreateDBusServiceWatcher,
               std::unique_ptr<ChromeosDBusServiceWatcher>(
                   scoped_refptr<dbus::Bus> bus,
                   const std::string& connection_name,
                   const base::Closure& on_connection_vanish));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDBusServiceWatcherFactory);
};

}  // namespace shill

#endif  // SHILL_DBUS_MOCK_DBUS_SERVICE_WATCHER_FACTORY_H_
