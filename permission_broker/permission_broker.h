// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_PERMISSION_BROKER_H_
#define PERMISSION_BROKER_PERMISSION_BROKER_H_

#include <dbus/dbus.h>
#include <string>

#include <base/macros.h>

#include "permission_broker/dbus_adaptors/org.chromium.PermissionBroker.h"
#include "permission_broker/rule_engine.h"

namespace permission_broker {

// The PermissionBroker encapsulates the execution of a chain of Rules which
// decide whether or not to grant access to a given path. The PermissionBroker
// is also responsible for providing a DBus interface to clients.
class PermissionBroker : public org::chromium::PermissionBrokerAdaptor,
                         public org::chromium::PermissionBrokerInterface {
 public:
  PermissionBroker(const scoped_refptr<dbus::Bus>& bus,
                   const std::string& access_group,
                   const std::string& udev_run_path,
                   int poll_interval_msecs);
  ~PermissionBroker();

  // Register the DBus object and interfaces.
  void RegisterAsync(
        const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction& cb);

 private:
  // The method exposed on DBus.
  bool RequestPathAccess(const std::string& in_path,
                         int32_t in_interface_id) override;

  RuleEngine rule_engine_;
  chromeos::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBroker);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_PERMISSION_BROKER_H_
