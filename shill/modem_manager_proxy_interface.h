// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_MANAGER_PROXY_INTERFACE_
#define SHILL_MODEM_MANAGER_PROXY_INTERFACE_

#include <vector>

#include <dbus-c++/types.h>

namespace shill {

// These are the methods that a ModemManager proxy must support. The interface
// is provided so that it can be mocked in tests.
class ModemManagerProxyInterface {
 public:
  virtual ~ModemManagerProxyInterface() {}

  virtual std::vector<DBus::Path> EnumerateDevices() = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_MANAGER_PROXY_INTERFACE_
