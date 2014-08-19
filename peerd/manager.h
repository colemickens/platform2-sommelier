// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_H_
#define PEERD_MANAGER_H_

#include <string>

#include <base/basictypes.h>

#include "peerd/manager_dbus_proxy.h"
#include "peerd/typedefs.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace peerd {

// Manages global state of peerd.
class Manager {
 public:
  explicit Manager(const scoped_refptr<dbus::Bus>& bus);
  virtual ~Manager() = default;
  void Init(const OnInitFinish& success_cb);

  // DBus handlers
  std::string Ping();

 private:
  ManagerDBusProxy proxy_;

  friend class ManagerDBusProxyTest;
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace peerd

#endif  // PEERD_MANAGER_H_
