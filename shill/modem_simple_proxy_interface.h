// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_SIMPLE_PROXY_INTERFACE_
#define SHILL_MODEM_SIMPLE_PROXY_INTERFACE_

#include "shill/dbus_properties.h"

namespace shill {

class AsyncCallHandler;
class Error;

// These are the methods that a ModemManager.Modem.Simple proxy must
// support. The interface is provided so that it can be mocked in tests.
// All calls are made asynchronously. Call completion is signalled through
// the corresponding 'OnXXXCallback' method in the ProxyDelegate interface.
class ModemSimpleProxyInterface {
 public:
  virtual ~ModemSimpleProxyInterface() {}

  virtual void GetModemStatus(AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void Connect(const DBusPropertiesMap &properties,
                       AsyncCallHandler *call_handler, int timeout) = 0;
};

// ModemManager.Modem.Simple method reply callback and signal
// delegate to be associated with the proxy.
class ModemSimpleProxyDelegate {
 public:
  virtual ~ModemSimpleProxyDelegate() {}

  virtual void OnGetModemStatusCallback(const DBusPropertiesMap &props,
                                        const Error &error,
                                        AsyncCallHandler *call_handler) = 0;
  virtual void OnConnectCallback(const Error &error,
                                 AsyncCallHandler *call_handler) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_SIMPLE_PROXY_INTERFACE_
