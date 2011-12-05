// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_PROXY_INTERFACE_
#define SHILL_MODEM_PROXY_INTERFACE_

#include <string>

#include <dbus-c++/types.h>

#include "shill/dbus_properties.h"

namespace shill {

class AsyncCallHandler;
class Error;

typedef DBus::Struct<std::string, std::string, std::string> ModemHardwareInfo;

// These are the methods that a ModemManager.Modem proxy must support. The
// interface is provided so that it can be mocked in tests. All calls are
// made asynchronously. Call completion is signalled through the corresponding
// 'OnXXXCallback' method in the ProxyDelegate interface.
class ModemProxyInterface {
 public:
  virtual ~ModemProxyInterface() {}

  virtual void Enable(bool enable, AsyncCallHandler *call_handler,
                      int timeout) = 0;
  // TODO(ers): temporarily advertise the blocking version
  // of Enable, until Cellular::Stop is converted for async.
  virtual void Enable(bool enable) = 0;
  virtual void Disconnect() = 0;
  virtual void GetModemInfo(AsyncCallHandler *call_handler, int timeout) = 0;
};

// ModemManager.Modem signal and method callback delegate
// to be associated with the proxy.
class ModemProxyDelegate {
 public:
  virtual ~ModemProxyDelegate() {}

  virtual void OnModemStateChanged(
      uint32 old_state, uint32 new_state, uint32 reason) = 0;

  // ModemProxyInterface::Enable callback.
  virtual void OnModemEnableCallback(const Error &error,
                                     AsyncCallHandler *call_handler) = 0;
  virtual void OnGetModemInfoCallback(const ModemHardwareInfo &info,
                                      const Error &error,
                                      AsyncCallHandler *call_handler) = 0;
  virtual void OnDisconnectCallback(const Error &error,
                                    AsyncCallHandler *call_handler) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_PROXY_INTERFACE_
