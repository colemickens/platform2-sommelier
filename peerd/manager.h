// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_H_
#define PEERD_MANAGER_H_

#include <base/basictypes.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <gtest/gtest_prod.h>

#include "peerd/typedefs.h"

namespace peerd {

// Manages global state of peerd.
class Manager {
 public:
  explicit Manager(const scoped_refptr<dbus::Bus>& bus);
  ~Manager();

  void Init(const OnInitFinish& success_cb);

 private:
  // Response to Ping method invocations via DBus.
  void HandlePing(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // weak; owned by the Bus object.

  FRIEND_TEST(ManagerTest, PingReturnsHelloWorld);
  FRIEND_TEST(ManagerTest, RejectPingWithArgs);
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace peerd

#endif  // PEERD_MANAGER_H_
