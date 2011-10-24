// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_CARD_PROXY_INTERFACE_
#define SHILL_MODEM_GSM_CARD_PROXY_INTERFACE_

#include <string>

namespace shill {

// These are the methods that a ModemManager.Modem.Gsm.Card proxy must
// support. The interface is provided so that it can be mocked in tests.
class ModemGSMCardProxyInterface {
 public:
  virtual ~ModemGSMCardProxyInterface() {}

  virtual std::string GetIMEI() = 0;
  virtual std::string GetIMSI() = 0;
  virtual std::string GetSPN() = 0;
  virtual std::string GetMSISDN() = 0;

  virtual void EnablePIN(const std::string &pin, bool enabled) = 0;
  virtual void SendPIN(const std::string &pin) = 0;
  virtual void SendPUK(const std::string &puk, const std::string &pin) = 0;
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin) = 0;
};

// ModemManager.Modem.Gsm.Card callback delegate to be associated with the
// proxy.
class ModemGSMCardProxyDelegate {
 public:
  virtual ~ModemGSMCardProxyDelegate() {}
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_CARD_PROXY_INTERFACE_
