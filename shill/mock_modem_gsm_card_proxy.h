// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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

  MOCK_METHOD0(GetIMEI, std::string());
  MOCK_METHOD0(GetIMSI, std::string());
  MOCK_METHOD0(GetSPN, std::string());
  MOCK_METHOD0(GetMSISDN, std::string());

  MOCK_METHOD2(EnablePIN, void(const std::string &pin, bool enabled));
  MOCK_METHOD1(SendPIN, void(const std::string &pin));
  MOCK_METHOD2(SendPUK, void(const std::string &puk, const std::string &pin));
  MOCK_METHOD2(ChangePIN,
               void(const std::string &old_pin, const std::string &new_pin));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGSMCardProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_GSM_CARD_PROXY_H_
