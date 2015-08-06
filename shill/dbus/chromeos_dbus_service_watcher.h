// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_DBUS_SERVICE_WATCHER_H_
#define SHILL_DBUS_CHROMEOS_DBUS_SERVICE_WATCHER_H_

#include <string>

#include <chromeos/dbus/dbus_service_watcher.h>

#include "shill/rpc_service_watcher_interface.h"

namespace shill {

// Wrapper for chromeos::dbus::DBusServiceWatcher for monitoring remote
// DBus service.
class ChromeosDBusServiceWatcher : public RPCServiceWatcherInterface {
 public:
  ChromeosDBusServiceWatcher(
      scoped_refptr<dbus::Bus> bus,
      const std::string& connection_name,
      const base::Closure& on_connection_vanished);
  ~ChromeosDBusServiceWatcher() override;

 private:
  std::unique_ptr<chromeos::dbus_utils::DBusServiceWatcher> watcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosDBusServiceWatcher);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DBUS_SERVICE_WATCHER_H_
