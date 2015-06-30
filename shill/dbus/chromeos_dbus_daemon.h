// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_DBUS_DAEMON_H_
#define SHILL_DBUS_CHROMEOS_DBUS_DAEMON_H_

#include <chromeos/daemons/dbus_daemon.h>

#include "shill/chromeos_daemon.h"

namespace shill {

class ChromeosDBusDaemon : public chromeos::DBusServiceDaemon,
                           public ChromeosDaemon {
 public:
  explicit ChromeosDBusDaemon(const Settings& settings, Config* config);
  ~ChromeosDBusDaemon() = default;

  // Implementation of ChromeosDaemon.
  void RunMessageLoop() override;

 protected:
  // Implementation of chromeos::DBusServiceDaemon.
  int OnInit() override;
  void OnShutdown(int* return_code) override;
  void RegisterDBusObjectsAsync(
      chromeos::dbus_utils::AsyncEventSequencer* sequencer) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeosDBusDaemon);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DBUS_DAEMON_H_
