// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_NETWORK_PROXY_INTERFACE_
#define SHILL_MODEM_GSM_NETWORK_PROXY_INTERFACE_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/types.h>

namespace shill {

class AsyncCallHandler;
class Error;

typedef DBus::Struct<unsigned int, std::string, std::string>
    GSMRegistrationInfo;
typedef std::map<std::string, std::string> GSMScanResult;
typedef std::vector<GSMScanResult> GSMScanResults;

// These are the methods that a ModemManager.Modem.Gsm.Network proxy must
// support. The interface is provided so that it can be mocked in tests.
// All calls are made asynchronously. Call completion is signalled through
// the corresponding 'OnXXXCallback' method in the ProxyDelegate interface.
class ModemGSMNetworkProxyInterface {
 public:
  virtual ~ModemGSMNetworkProxyInterface() {}

  virtual void GetRegistrationInfo(AsyncCallHandler *call_handler,
                                   int timeout) = 0;
  virtual uint32 GetSignalQuality() = 0;
  virtual void Register(const std::string &network_id,
                        AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void Scan(AsyncCallHandler *call_handler, int timeout) = 0;

  // Properties.
  virtual uint32 AccessTechnology() = 0;
};

// ModemManager.Modem.Gsm.Network method reply callback and signal
// delegate to be associated with the proxy.
class ModemGSMNetworkProxyDelegate {
 public:
  virtual ~ModemGSMNetworkProxyDelegate() {}

  virtual void OnGSMNetworkModeChanged(uint32 mode) = 0;
  // The following callback handler is used for both the
  // RegistrationInfo signal and the GetRegistrationInfo
  // method reply. For the signal case, the |call_handler|
  // is NULL.
  virtual void OnGSMRegistrationInfoChanged(
      uint32 status,
      const std::string &operator_code,
      const std::string &operator_name,
      const Error &error,
      AsyncCallHandler *call_handler) = 0;
  virtual void OnGSMSignalQualityChanged(uint32 quality) = 0;
  virtual void OnScanCallback(const GSMScanResults &results,
                              const Error &error,
                              AsyncCallHandler *call_handler) = 0;
  virtual void OnRegisterCallback(const Error &error,
                                  AsyncCallHandler *call_handler) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_NETWORK_PROXY_INTERFACE_
