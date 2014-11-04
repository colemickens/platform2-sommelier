// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_DEV_MODE_NO_OWNER_RESTRICTION_H_
#define DEBUGD_SRC_DEV_MODE_NO_OWNER_RESTRICTION_H_

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

// Provides functionality to check that the system is in dev mode and has no
// owner. Used by RestrictedToolWrapper classes to limit access to tools.
class DevModeNoOwnerRestriction {
 public:
  explicit DevModeNoOwnerRestriction(DBus::Connection* system_dbus);

  virtual ~DevModeNoOwnerRestriction() = default;

  // Checks whether tool access is allowed.
  //
  // To get access to the tool, the system must be in dev mode with no owner
  // and the boot lockbox cannot be finalized.
  //
  // |error| can be NULL or a pointer to a DBus::Error object, in which case
  // it will be filled with a descriptive error message if tool access is
  // blocked.
  //
  // Returns true if tool access is allowed.
  bool AllowToolUse(DBus::Error* error);

 private:
  DBus::Connection* system_dbus_;

  // Virtual member functions to allow overrides for testing.
  virtual bool InDevMode();
  virtual bool GetOwnerAndLockboxStatus(bool* owner_user_exists,
                                        bool* boot_lockbox_finalized);

  DISALLOW_COPY_AND_ASSIGN(DevModeNoOwnerRestriction);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_DEV_MODE_NO_OWNER_RESTRICTION_H_
