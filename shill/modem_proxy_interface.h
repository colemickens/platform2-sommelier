// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_PROXY_INTERFACE_
#define SHILL_MODEM_PROXY_INTERFACE_

#include <dbus-c++/types.h>

namespace shill {

// These are the methods that a ModemManager.Modem proxy must support. The
// interface is provided so that it can be mocked in tests.
class ModemProxyInterface {
 public:
  typedef DBus::Struct<std::string, std::string, std::string> Info;

  virtual ~ModemProxyInterface() {}

  virtual void Enable(const bool enable) = 0;
  virtual Info GetInfo() = 0;
};

// ModemManager.Modem signal listener to be associated with the proxy.
class ModemProxyListener {
 public:
  virtual ~ModemProxyListener() {}

  virtual void OnModemStateChanged(uint32 old_state,
                                   uint32 new_state,
                                   uint32 reason) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_PROXY_INTERFACE_
