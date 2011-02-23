// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CONTROL_
#define SHILL_DBUS_CONTROL_

#include "shill/control_interface.h"

namespace shill {
// This is the Interface for the control channel for Shill.
class DBusControl : public ControlInterface {
 public:
  ManagerProxyInterface *CreateManagerProxy(Manager *manager);
};

}  // namespace shill

#endif  // SHILL_MANAGER_
