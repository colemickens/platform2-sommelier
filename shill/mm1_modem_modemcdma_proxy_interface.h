// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_MODEMCDMA_PROXY_INTERFACE_
#define SHILL_MM1_MODEM_MODEMCDMA_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_properties.h"

namespace shill {
class AsyncCallHandler;
class Error;

namespace mm1 {

// These are the methods that a
// org.freedesktop.ModemManager1.Modem.ModemCdma proxy must support.
// The interface is provided so that it can be mocked in tests.  All
// calls are made asynchronously. Call completion is signalled through
// the corresponding 'OnXXXCallback' method in the ProxyDelegate
// interface.
class ModemModemCdmaProxyInterface {
 public:
  virtual ~ModemModemCdmaProxyInterface() {}

  virtual void Activate(const std::string &carrier,
                        AsyncCallHandler *call_handler,
                        int timeout) = 0;
  virtual void ActivateManual(
      const DBusPropertiesMap &properties,
      AsyncCallHandler *call_handler, int timeout) = 0;


  // Properties.
  virtual std::string Meid() = 0;
  virtual std::string Esn() = 0;
  virtual uint32_t Sid() = 0;
  virtual uint32_t Nid() = 0;
  virtual uint32_t Cdma1xRegistrationState() = 0;
  virtual uint32_t EvdoRegistrationState() = 0;
};


// ModemManager1.Modem.ModemCdma signal delegate to be associated with
// the proxy.
class ModemModemCdmaProxyDelegate {
 public:
  virtual ~ModemModemCdmaProxyDelegate() {}

  // handle signals
  virtual void OnActivationStateChanged(
      const uint32_t &activation_state,
      const uint32_t &activation_error,
      const DBusPropertiesMap &status_changes) = 0;

  // Handle async callbacks
  virtual void OnActivateCallback(const Error &error,
                                  AsyncCallHandler *call_handler) = 0;
  virtual void OnActivateManualCallback(
      const Error &error,
      AsyncCallHandler *call_handler) = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_MODEMCDMA_PROXY_INTERFACE_
