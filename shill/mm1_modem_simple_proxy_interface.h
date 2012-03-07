// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_SIMPLE_PROXY_INTERFACE_
#define SHILL_MM1_MODEM_SIMPLE_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/callbacks.h"

namespace shill {
class Error;

namespace mm1 {

// These are the methods that a
// org.freedesktop.ModemManager1.Modem.Simple proxy must support.  The
// interface is provided so that it can be mocked in tests.  All calls
// are made asynchronously. Call completion is signalled via the callbacks
// passed to the methods.
class ModemSimpleProxyInterface {
 public:
  virtual ~ModemSimpleProxyInterface() {}

  virtual void Connect(const DBusPropertiesMap &properties,
                       Error *error,
                       const DBusPathCallback &callback,
                       int timeout) = 0;
  virtual void Disconnect(const ::DBus::Path &bearer,
                          Error *error,
                          const ResultCallback &callback,
                          int timeout) = 0;
  virtual void GetStatus(Error *error,
                         const DBusPropertyMapCallback &callback,
                         int timeout) = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_SIMPLE_PROXY_INTERFACE_
