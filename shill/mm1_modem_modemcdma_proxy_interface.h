// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_MODEMCDMA_PROXY_INTERFACE_
#define SHILL_MM1_MODEM_MODEMCDMA_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/callbacks.h"

namespace shill {
class Error;

namespace mm1 {

// These are the methods that a
// org.freedesktop.ModemManager1.Modem.ModemCdma proxy must support.
// The interface is provided so that it can be mocked in tests.  All
// calls are made asynchronously. Call completion is signalled via
// the callbacks passed to the methods.
class ModemModemCdmaProxyInterface {
 public:
  virtual ~ModemModemCdmaProxyInterface() {}

  virtual void Activate(const std::string &carrier,
                        Error *error,
                        const ResultCallback &callback,
                        int timeout) = 0;
  virtual void ActivateManual(
      const DBusPropertiesMap &properties,
      Error *error,
      const ResultCallback &callback,
      int timeout) = 0;

  virtual void set_activation_state_callback(
      const ActivationStateSignalCallback &callback) = 0;

  // Properties.
  virtual std::string Meid() = 0;
  virtual std::string Esn() = 0;
  virtual uint32_t Sid() = 0;
  virtual uint32_t Nid() = 0;
  virtual uint32_t Cdma1xRegistrationState() = 0;
  virtual uint32_t EvdoRegistrationState() = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_MODEMCDMA_PROXY_INTERFACE_
