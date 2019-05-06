// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_DBUS_SERVICE_WATCHER_H_
#define SHILL_DBUS_CHROMEOS_DBUS_SERVICE_WATCHER_H_

#include <memory>
#include <string>

#include <brillo/dbus/dbus_service_watcher.h>

namespace shill {

// Wrapper for brillo::dbus::DBusServiceWatcher for monitoring remote
// DBus service.
class ChromeosDBusServiceWatcher {
 public:
  ChromeosDBusServiceWatcher(
      scoped_refptr<dbus::Bus> bus,
      const std::string& connection_name,
      const base::Closure& on_connection_vanished);
  ~ChromeosDBusServiceWatcher();

 protected:
  ChromeosDBusServiceWatcher() = default;  // for mocking.

 private:
  std::unique_ptr<brillo::dbus_utils::DBusServiceWatcher> watcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosDBusServiceWatcher);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DBUS_SERVICE_WATCHER_H_
