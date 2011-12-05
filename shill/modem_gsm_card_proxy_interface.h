// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_CARD_PROXY_INTERFACE_
#define SHILL_MODEM_GSM_CARD_PROXY_INTERFACE_

#include <string>

namespace shill {

class AsyncCallHandler;
class Error;

// These are the methods that a ModemManager.Modem.Gsm.Card proxy must
// support. The interface is provided so that it can be mocked in tests.
// All calls are made asynchronously. Call completion is signalled through
// the corresponding 'OnXXXCallback' method in the ProxyDelegate interface.
class ModemGSMCardProxyInterface {
 public:
  virtual ~ModemGSMCardProxyInterface() {}

  virtual void GetIMEI(AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void GetIMSI(AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void GetSPN(AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void GetMSISDN(AsyncCallHandler *call_handler, int timeout) = 0;

  virtual void EnablePIN(const std::string &pin, bool enabled,
                         AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void SendPIN(const std::string &pin,
                       AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void SendPUK(const std::string &puk, const std::string &pin,
                       AsyncCallHandler *call_handler, int timeout) = 0;
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         AsyncCallHandler *call_handler, int timeout) = 0;
};

// ModemManager.Modem.Gsm.Card callback delegate to be associated with the
// proxy.
class ModemGSMCardProxyDelegate {
 public:
  virtual ~ModemGSMCardProxyDelegate() {}

  virtual void OnGetIMEICallback(const std::string &imei,
                                 const Error &e,
                                 AsyncCallHandler *call_handler) = 0;
  virtual void OnGetIMSICallback(const std::string &imsi,
                                 const Error &e,
                                 AsyncCallHandler *call_handler) = 0;
  virtual void OnGetSPNCallback(const std::string &spn,
                                const Error &e,
                                AsyncCallHandler *call_handler) = 0;
  virtual void OnGetMSISDNCallback(const std::string &misisdn,
                                   const Error &e,
                                   AsyncCallHandler *call_handler) = 0;
  // Callback used for all four PIN operations: EnablePIN, SendPIN,
  // SendPUK, and ChangePIN.
  virtual void OnPINOperationCallback(const Error &e,
                                      AsyncCallHandler *call_handler) = 0;
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_CARD_PROXY_INTERFACE_
