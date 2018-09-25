// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_GOBI_PROXY_INTERFACE_H_
#define SHILL_CELLULAR_MODEM_GOBI_PROXY_INTERFACE_H_

#include <string>

#include "shill/callbacks.h"

namespace shill {

class Error;

// These are the methods that a ModemManager.Modem.Gobi proxy must support. The
// interface is provided so that it can be mocked in tests.  All calls are made
// asynchronously.
class ModemGobiProxyInterface {
 public:
  virtual ~ModemGobiProxyInterface() {}

  virtual void SetCarrier(const std::string& carrier,
                          Error* error, const ResultCallback& callback,
                          int timeout) = 0;
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_GOBI_PROXY_INTERFACE_H_
