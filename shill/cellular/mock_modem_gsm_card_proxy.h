// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MODEM_GSM_CARD_PROXY_H_
#define SHILL_CELLULAR_MOCK_MODEM_GSM_CARD_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/modem_gsm_card_proxy_interface.h"

namespace shill {

class MockModemGSMCardProxy : public ModemGSMCardProxyInterface {
 public:
  MockModemGSMCardProxy();
  ~MockModemGSMCardProxy() override;

  MOCK_METHOD3(GetIMEI, void(Error* error,
                             const GSMIdentifierCallback& callback,
                             int timeout));
  MOCK_METHOD3(GetIMSI, void(Error* error,
                             const GSMIdentifierCallback& callback,
                             int timeout));
  MOCK_METHOD3(GetSPN, void(Error* error,
                            const GSMIdentifierCallback& callback,
                            int timeout));
  MOCK_METHOD3(GetMSISDN, void(Error* error,
                               const GSMIdentifierCallback& callback,
                               int timeout));

  MOCK_METHOD5(EnablePIN, void(const std::string& pin, bool enabled,
                               Error* error, const ResultCallback& callback,
                               int timeout));
  MOCK_METHOD4(SendPIN, void(const std::string& pin,
                             Error* error, const ResultCallback& callback,
                             int timeout));
  MOCK_METHOD5(SendPUK, void(const std::string& puk, const std::string& pin,
                             Error* error, const ResultCallback& callback,
                             int timeout));
  MOCK_METHOD5(ChangePIN, void(const std::string& old_pin,
                               const std::string& new_pin,
                               Error* error, const ResultCallback& callback,
                               int timeout));
  MOCK_METHOD0(EnabledFacilityLocks, uint32_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGSMCardProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MODEM_GSM_CARD_PROXY_H_
