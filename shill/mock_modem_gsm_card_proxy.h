// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_GSM_CARD_PROXY_H_
#define SHILL_MOCK_MODEM_GSM_CARD_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_gsm_card_proxy_interface.h"

namespace shill {

class MockModemGSMCardProxy : public ModemGSMCardProxyInterface {
 public:
  MockModemGSMCardProxy();
  virtual ~MockModemGSMCardProxy();

  MOCK_METHOD2(GetIMEI, void(AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD2(GetIMSI, void(AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD2(GetSPN, void(AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD2(GetMSISDN, void(AsyncCallHandler *call_handler, int timeout));

  MOCK_METHOD4(EnablePIN, void(const std::string &pin, bool enabled,
                               AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD3(SendPIN, void(const std::string &pin,
                             AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD4(SendPUK, void(const std::string &puk, const std::string &pin,
                             AsyncCallHandler *call_handler, int timeout));
  MOCK_METHOD4(ChangePIN, void(const std::string &old_pin,
                               const std::string &new_pin,
                               AsyncCallHandler *call_handler, int timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGSMCardProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_GSM_CARD_PROXY_H_
