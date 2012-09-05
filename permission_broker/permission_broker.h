// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_PERMISSION_BROKER_H_
#define PERMISSION_BROKER_PERMISSION_BROKER_H_

#include <dbus/dbus.h>
#include <libudev.h>

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace permission_broker {

class Rule;

// The PermissionBroker encapsulates the execution of a chain of Rules which
// decide whether or not to grant access to a given path. The PermissionBroker
// is also responsible for providing a DBus interface to clients.
class PermissionBroker {
 public:
  PermissionBroker();
  ~PermissionBroker();

  // Initializes the broker and loops waiting for requests on the DBus
  // interface. Never returns.
  void Run();

  // Adds |rule| to the end of the existing rule chain. Takes ownership of
  // |rule|.
  void AddRule(Rule *rule);

 private:
  friend class PermissionBrokerTest;

  // The callback invoked by the DBus method handler in order to dispatch method
  // calls to their individual handlers.
  static DBusHandlerResult MainDBusMethodHandler(DBusConnection *connection,
                                                 DBusMessage *message,
                                                 void *data);

  // Waits for all queued udev events to complete before returning. Is
  // equivalent to invoking 'udevadm settle', but without the external
  // dependency and overhead.
  void WaitForEmptyUdevQueue();

  // Invokes each of the rules in order on |path| until either a rule explicitly
  // denies access to the path or until there are no more rules left. If, after
  // executing all of the stored rules, no rule has explicitly allowed access to
  // the path then access is denied. If _any_ rule denies access to |path| then
  // processing the rules is aborted early and access is denied.
  bool ProcessPath(const std::string &path);

  // Grants access to |path|, which is accomplished by changing the owning group
  // on the path to the one specified numerically by the 'access_group' flag.
  virtual bool GrantAccess(const std::string &path);

  // Given |vendor_id| and |product_id|, scans the USB subsystem for devices
  // whose idVendor and idProduct attributes match and inserts their device node
  // paths into |path|, clearing |paths| first.
  bool ExpandUsbIdentifiersToPaths(const uint16_t vendor_id,
                                   const uint16_t product_id,
                                   std::vector<std::string> *paths);

  DBusMessage *HandleRequestPathAccessMethod(DBusMessage *message);
  DBusMessage *HandleRequestUsbAccessMethod(DBusMessage *message);

  struct udev *udev_;
  std::vector<Rule *> rules_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBroker);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_PERMISSION_BROKER_H_
