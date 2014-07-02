// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_PROXY_INTERFACE_
#define SHILL_MODEM_PROXY_INTERFACE_

#include <string>

#include <dbus-c++/types.h>

#include "shill/callbacks.h"
#include "shill/dbus_properties.h"

namespace shill {

class CallContext;
class Error;

typedef DBus::Struct<std::string, std::string, std::string> ModemHardwareInfo;

typedef base::Callback<void(uint32,
                            uint32, uint32)> ModemStateChangedSignalCallback;
typedef base::Callback<void(const ModemHardwareInfo &,
                            const Error &)> ModemInfoCallback;

// These are the methods that a ModemManager.Modem proxy must support. The
// interface is provided so that it can be mocked in tests. All calls are
// made asynchronously.
class ModemProxyInterface {
 public:
  virtual ~ModemProxyInterface() {}

  virtual void Enable(bool enable, Error *error,
                      const ResultCallback &callback, int timeout) = 0;
  virtual void Disconnect(Error *error, const ResultCallback &callback,
                          int timeout) = 0;
  virtual void GetModemInfo(Error *error, const ModemInfoCallback &callback,
                            int timeout) = 0;

  virtual void set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_PROXY_INTERFACE_
