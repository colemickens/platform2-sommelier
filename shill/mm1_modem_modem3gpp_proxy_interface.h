// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_MODEM3GPP_PROXY_INTERFACE_
#define SHILL_MM1_MODEM_MODEM3GPP_PROXY_INTERFACE_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_properties.h"

namespace shill {
class AsyncCallHandler;
class Error;

namespace mm1 {

// These are the methods that a
// org.freedesktop.ModemManager1.Modem.Modem3gpp proxy must support.

// The interface is provided so that it can be mocked in tests.
// All calls are made asynchronously. Call completion is signalled through
// the corresponding 'OnXXXCallback' method in the ProxyDelegate interface.
class ModemModem3gppProxyInterface {
 public:
  virtual ~ModemModem3gppProxyInterface() {}

  virtual void Register(const std::string &operator_id,
                        AsyncCallHandler *call_handler,
                        int timeout) = 0;
  virtual void Scan(AsyncCallHandler *call_handler, int timeout) = 0;

  // Properties.
  virtual std::string Imei() = 0;
  virtual uint32_t RegistrationState() = 0;
  virtual std::string OperatorCode() = 0;
  virtual std::string OperatorName() = 0;
  virtual uint32_t EnabledFacilityLocks() = 0;
};

// ModemManager1.Modem.Modem3gpp signal delegate to be associated with
// the proxy.
class ModemModem3gppProxyDelegate {
 public:
  virtual ~ModemModem3gppProxyDelegate() {}

  // Handle async callbacks
  virtual void OnRegisterCallback(const Error &error,
                                  AsyncCallHandler *call_handler) = 0;
  virtual void OnScanCallback(
      const std::vector<DBusPropertiesMap> &results,
      const Error &error,
      AsyncCallHandler *call_handler) = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_MODEM3GPP_PROXY_INTERFACE_
