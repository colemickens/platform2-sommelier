// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_PROXY_INTERFACE_H_
#define SHILL_CELLULAR_MODEM_PROXY_INTERFACE_H_

#include <string>

#include "shill/callbacks.h"

namespace shill {

class CallContext;
class Error;

typedef base::Callback<void(uint32_t, uint32_t, uint32_t)>
    ModemStateChangedSignalCallback;
typedef base::Callback<void(const std::string& manufacturer,
                            const std::string& modem,
                            const std::string& version,
                            const Error&)> ModemInfoCallback;

// These are the methods that a ModemManager.Modem proxy must support. The
// interface is provided so that it can be mocked in tests. All calls are
// made asynchronously.
class ModemProxyInterface {
 public:
  virtual ~ModemProxyInterface() {}

  virtual void Enable(bool enable, Error* error,
                      const ResultCallback& callback, int timeout) = 0;
  virtual void Disconnect(Error* error, const ResultCallback& callback,
                          int timeout) = 0;
  virtual void GetModemInfo(Error* error, const ModemInfoCallback& callback,
                            int timeout) = 0;

  virtual void set_state_changed_callback(
      const ModemStateChangedSignalCallback& callback) = 0;
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_PROXY_INTERFACE_H_
