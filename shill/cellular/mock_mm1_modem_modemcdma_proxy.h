// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MM1_MODEM_MODEMCDMA_PROXY_H_
#define SHILL_CELLULAR_MOCK_MM1_MODEM_MODEMCDMA_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/mm1_modem_modemcdma_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockModemModemCdmaProxy : public ModemModemCdmaProxyInterface {
 public:
  MockModemModemCdmaProxy();
  ~MockModemModemCdmaProxy() override;

  MOCK_METHOD4(Activate,
               void(const std::string& carrier,
                    Error* error,
                    const ResultCallback& callback,
                    int timeout));

  MOCK_METHOD4(ActivateManual,
               void(const KeyValueStore& properties,
                    Error* error,
                    const ResultCallback& callback,
                    int timeout));

  MOCK_METHOD1(set_activation_state_callback,
               void(const ActivationStateSignalCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemModemCdmaProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MM1_MODEM_MODEMCDMA_PROXY_H_
