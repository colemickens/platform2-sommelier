//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DBUS_CHROMEOS_DBUS_DAEMON_H_
#define SHILL_DBUS_CHROMEOS_DBUS_DAEMON_H_

#include <base/callback.h>
#include <chromeos/daemons/dbus_daemon.h>

#include "shill/chromeos_daemon.h"
#include "shill/event_dispatcher.h"

namespace shill {

class ChromeosDBusDaemon : public chromeos::DBusServiceDaemon,
                           public ChromeosDaemon {
 public:
  ChromeosDBusDaemon(const base::Closure& startup_callback,
                     const Settings& settings,
                     Config* config);
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
  // Invoked when DBus service is registered with the bus.  This function will
  // request the ownership for our DBus service.
  void OnDBusServiceRegistered(
      const base::Callback<void(bool)>& completion_action,
      bool success);

  // Invoke when shill completes its termination tasks during shutdown.
  void OnTerminationCompleted();

  EventDispatcher dispatcher_;
  base::Closure startup_callback_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosDBusDaemon);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DBUS_DAEMON_H_
