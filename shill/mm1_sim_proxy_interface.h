// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_SIM_PROXY_INTERFACE_
#define SHILL_MM1_SIM_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/callbacks.h"

namespace shill {

class Error;

namespace mm1 {

// These are the methods that a org.freedesktop.ModemManager1.Sim
// proxy must support. The interface is provided so that it can be
// mocked in tests. All calls are made asynchronously. Call completion
// is signalled via the callbacks passed to the methods.
class SimProxyInterface {
 public:
  virtual ~SimProxyInterface() {}

  virtual void SendPin(const std::string &pin,
                       Error *error,
                       const ResultCallback &callback,
                       int timeout) = 0;
  virtual void SendPuk(const std::string &puk,
                       const std::string &pin,
                       Error *error,
                       const ResultCallback &callback,
                       int timeout) = 0;
  virtual void EnablePin(const std::string &pin,
                         const bool enabled,
                         Error *error,
                         const ResultCallback &callback,
                         int timeout) = 0;
  virtual void ChangePin(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error,
                         const ResultCallback &callback,
                         int timeout) = 0;

  // Properties.
  virtual const std::string SimIdentifier() = 0;
  virtual const std::string Imsi() = 0;
  virtual const std::string OperatorIdentifier() = 0;
  virtual const std::string OperatorName() = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_SIM_PROXY_INTERFACE_
