// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MODEM_CDMA_PROXY_H_
#define SHILL_CELLULAR_MOCK_MODEM_CDMA_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/modem_cdma_proxy_interface.h"

namespace shill {

class MockModemCdmaProxy : public ModemCdmaProxyInterface {
 public:
  MockModemCdmaProxy();
  ~MockModemCdmaProxy() override;

  MOCK_METHOD4(Activate, void(const std::string& carrier, Error* error,
                              const ActivationResultCallback& callback,
                              int timeout));
  MOCK_METHOD3(GetRegistrationState,
               void(Error* error, const RegistrationStateCallback& callback,
                    int timeout));
  MOCK_METHOD3(GetSignalQuality, void(Error* error,
                                      const SignalQualityCallback& callback,
                                      int timeout));
  MOCK_METHOD0(MEID, const std::string());
  MOCK_METHOD1(set_activation_state_callback,
      void(const ActivationStateSignalCallback& callback));
  MOCK_METHOD1(set_signal_quality_callback,
      void(const SignalQualitySignalCallback& callback));
  MOCK_METHOD1(set_registration_state_callback,
      void(const RegistrationStateSignalCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemCdmaProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MODEM_CDMA_PROXY_H_
