// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_INIT_DAEMON_CONTROLLER_H_
#define LOGIN_MANAGER_INIT_DAEMON_CONTROLLER_H_

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>

namespace dbus {
class ObjectProxy;
class Response;
}

namespace login_manager {

class InitDaemonController {
 public:
  // Different triggering modes.
  enum class TriggerMode {
    // Wait for the impulse to be fully processed before returning.
    SYNC,
    // Asynchronously trigger the impulse.
    ASYNC,
  };

  virtual ~InitDaemonController() {}

  // Asks the init daemon to emit a signal (Upstart) or start a unit (systemd).
  // The response is null if the request failed or |mode| is ASYNC.
  virtual scoped_ptr<dbus::Response> TriggerImpulse(
      const std::string& name,
      const std::vector<std::string>& args_keyvals,
      TriggerMode mode) = 0;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_INIT_DAEMON_CONTROLLER_H_
