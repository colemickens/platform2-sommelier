// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_
#define SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/modem_gsm_network_proxy_interface.h"

namespace shill {

class MockModemGSMNetworkProxy : public ModemGSMNetworkProxyInterface {
 public:
  MockModemGSMNetworkProxy();
  virtual ~MockModemGSMNetworkProxy();

  MOCK_METHOD3(GetRegistrationInfo,
               void(Error *error, const RegistrationInfoCallback &callback,
                    int timeout));
  MOCK_METHOD3(GetSignalQuality, void(Error *error,
                                      const SignalQualityCallback &callback,
                                      int timeout));
  MOCK_METHOD4(Register, void(const std::string &network_id, Error *error,
                              const ResultCallback &callback, int timeout));
  MOCK_METHOD3(Scan, void(Error *error, const ScanResultsCallback &callback,
                          int timeout));
  MOCK_METHOD0(AccessTechnology, uint32());
  MOCK_METHOD1(set_signal_quality_callback,
      void(const SignalQualitySignalCallback &callback));
  MOCK_METHOD1(set_network_mode_callback,
      void(const NetworkModeSignalCallback &callback));
  MOCK_METHOD1(set_registration_info_callback,
      void(const RegistrationInfoSignalCallback &callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemGSMNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_GSM_NETWORK_PROXY_H_
