// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_SIMPLE_PROXY_INTERFACE_H_
#define SHILL_CELLULAR_MODEM_SIMPLE_PROXY_INTERFACE_H_

#include "shill/callbacks.h"

namespace shill {

class Error;

// These are the methods that a ModemManager.Modem.Simple proxy must
// support. The interface is provided so that it can be mocked in tests.
// All calls are made asynchronously.
class ModemSimpleProxyInterface {
 public:
  virtual ~ModemSimpleProxyInterface() {}

  virtual void GetModemStatus(Error* error,
                              const KeyValueStoreCallback& callback,
                              int timeout) = 0;
  virtual void Connect(const KeyValueStore& properties,
                       Error* error, const ResultCallback& callback,
                       int timeout) = 0;
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_SIMPLE_PROXY_INTERFACE_H_
