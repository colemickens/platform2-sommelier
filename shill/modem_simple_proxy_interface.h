// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_SIMPLE_PROXY_INTERFACE_
#define SHILL_MODEM_SIMPLE_PROXY_INTERFACE_

#include "shill/dbus_properties.h"

namespace shill {

// These are the methods that a ModemManager.Modem.Simple proxy must
// support. The interface is provided so that it can be mocked in tests.
class ModemSimpleProxyInterface {
 public:
  virtual ~ModemSimpleProxyInterface() {}

  virtual DBusPropertiesMap GetStatus() = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_SIMPLE_PROXY_INTERFACE_
