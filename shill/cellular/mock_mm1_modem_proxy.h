// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_MM1_MODEM_PROXY_H_
#define SHILL_CELLULAR_MOCK_MM1_MODEM_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/cellular/mm1_modem_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockModemProxy : public ModemProxyInterface {
 public:
  MockModemProxy();
  ~MockModemProxy() override;

  // Inherited methods from ModemProxyInterface.
  MOCK_METHOD4(Enable, void(bool enable,
                            Error* error,
                            const ResultCallback& callback,
                            int timeout));
  MOCK_METHOD4(CreateBearer, void(const KeyValueStore& properties,
                                  Error* error,
                                  const RpcIdentifierCallback& callback,
                                  int timeout));
  MOCK_METHOD4(DeleteBearer, void(const RpcIdentifier& bearer,
                                  Error* error,
                                  const ResultCallback& callback,
                                  int timeout));
  MOCK_METHOD3(Reset, void(Error* error,
                           const ResultCallback& callback,
                           int timeout));
  MOCK_METHOD4(FactoryReset, void(const std::string& code,
                                  Error* error,
                                  const ResultCallback& callback,
                                  int timeout));
  MOCK_METHOD4(SetCurrentCapabilities, void(uint32_t capabilities,
                                            Error* error,
                                            const ResultCallback& callback,
                                            int timeout));
  MOCK_METHOD5(SetCurrentModes,
               void(uint32_t allowed_modes,
                    uint32_t preferred_mode,
                    Error* error,
                    const ResultCallback& callback,
                    int timeout));
  MOCK_METHOD4(SetCurrentBands, void(const std::vector<uint32_t>& bands,
                                     Error* error,
                                     const ResultCallback& callback,
                                     int timeout));
  MOCK_METHOD5(Command, void(const std::string& cmd,
                             uint32_t user_timeout,
                             Error* error,
                             const StringCallback& callback,
                             int timeout));
  MOCK_METHOD4(SetPowerState, void(uint32_t power_state,
                                   Error* error,
                                   const ResultCallback& callback,
                                   int timeout));
  MOCK_METHOD1(set_state_changed_callback, void(
      const ModemStateChangedSignalCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_MM1_MODEM_PROXY_H_
