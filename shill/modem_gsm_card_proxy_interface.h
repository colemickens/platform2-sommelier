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
};

// ModemManager.Modem.Gsm.Card callback listener to be associated with the
// proxy.
class ModemGSMCardProxyListener {
 public:
  virtual ~ModemGSMCardProxyListener() {}
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_CARD_PROXY_INTERFACE_
