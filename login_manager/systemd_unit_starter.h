// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEMD_UNIT_STARTER_H_
#define LOGIN_MANAGER_SYSTEMD_UNIT_STARTER_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include "login_manager/init_daemon_controller.h"


namespace dbus {
class ObjectProxy;
class Response;
}

namespace login_manager {

class SystemdUnitStarter : public InitDaemonController {
 public:
  static const char kServiceName[];
  static const char kPath[];
  static const char kInterface[];
  static const char kStartUnitMode[];
  static const char kStartUnitMethodName[];
  static const char kSetEnvironmentMethodName[];
  static const char kUnsetEnvironmentMethodName[];

  explicit SystemdUnitStarter(dbus::ObjectProxy* proxy);
  virtual ~SystemdUnitStarter();
  virtual scoped_ptr<dbus::Response> StartUnit(
      const std::string& unit_name,
      const std::vector<std::string>& args_keyvals);
  scoped_ptr<dbus::Response> TriggerImpulse(
      const std::string &unit_name,
      const std::vector<std::string> &args_keyvals) final;
 private:
  dbus::ObjectProxy* systemd_dbus_proxy_;  // Weak, owned by caller.
  DISALLOW_COPY_AND_ASSIGN(SystemdUnitStarter);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEMD_UNIT_STARTER_H_
