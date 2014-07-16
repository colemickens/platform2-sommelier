// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_H_
#define PEERD_MANAGER_H_

#include <base/basictypes.h>
#include <dbus/bus.h>

namespace peerd {

// Manages global state of peerd.
class Manager {
 public:
  explicit Manager(scoped_refptr<dbus::Bus>);
  ~Manager();

 private:
  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // weak; owned by the Bus object.
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace peerd

#endif  // PEERD_MANAGER_H_
