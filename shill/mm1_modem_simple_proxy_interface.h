// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_SIMPLE_PROXY_INTERFACE_
#define SHILL_MM1_MODEM_SIMPLE_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_properties.h"

namespace shill {
class AsyncCallHandler;
class Error;

namespace mm1 {

// These are the methods that a
// org.freedesktop.ModemManager1.Modem.Simple proxy must support.  The
// interface is provided so that it can be mocked in tests.  All calls
// are made asynchronously. Call completion is signalled through the
// corresponding 'OnXXXCallback' method in the ProxyDelegate
// interface.
class ModemSimpleProxyInterface {
 public:
  virtual ~ModemSimpleProxyInterface() {}

  virtual void Connect(
      const DBusPropertiesMap &properties,
      AsyncCallHandler *call_handler,
      int timeout) = 0;
  virtual void Disconnect(const ::DBus::Path &bearer,
                          AsyncCallHandler *call_handler,
                          int timeout) = 0;
  virtual void GetStatus(AsyncCallHandler *call_handler,
                         int timeout) = 0;
};

// ModemManager1.Modem.Simple signal delegate to be associated with
// the proxy.
class ModemSimpleProxyDelegate {
 public:
  virtual ~ModemSimpleProxyDelegate() {}

  // Handle async callbacks
  virtual void OnConnectCallback(const ::DBus::Path &bearer,
                                 const Error &error,
                                 AsyncCallHandler *call_handler) = 0;
  virtual void OnDisconnectCallback(const Error &error,
                                    AsyncCallHandler *call_handler) = 0;
  virtual void OnGetStatusCallback(
      const DBusPropertiesMap &bearer,
      const Error &error,
      AsyncCallHandler *call_handler) = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_SIMPLE_PROXY_INTERFACE_
