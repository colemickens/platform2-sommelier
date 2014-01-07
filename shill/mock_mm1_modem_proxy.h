// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MOCK_MODEM_PROXY_H_
#define SHILL_MM1_MOCK_MODEM_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/mm1_modem_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockModemProxy : public ModemProxyInterface {
 public:
  MockModemProxy();
  virtual ~MockModemProxy();

  // Inherited methods from ModemProxyInterface.
  MOCK_METHOD4(Enable, void(bool enable,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout));
  MOCK_METHOD4(CreateBearer, void(const DBusPropertiesMap &properties,
                                  Error *error,
                                  const DBusPathCallback &callback,
                                  int timeout));
  MOCK_METHOD4(DeleteBearer, void(const ::DBus::Path &bearer,
                                  Error *error,
                                  const ResultCallback &callback,
                                  int timeout));
  MOCK_METHOD3(Reset, void(Error *error,
                           const ResultCallback &callback,
                           int timeout));
  MOCK_METHOD4(FactoryReset, void(const std::string &code,
                                  Error *error,
                                  const ResultCallback &callback,
                                  int timeout));
  MOCK_METHOD4(SetCurrentCapabilities, void(const uint32_t &capabilities,
                                            Error *error,
                                            const ResultCallback &callback,
                                            int timeout));
  MOCK_METHOD4(SetCurrentModes,
               void(const ::DBus::Struct<uint32_t, uint32_t> &modes,
                    Error *error,
                    const ResultCallback &callback,
                    int timeout));
  MOCK_METHOD4(SetCurrentBands, void(const std::vector<uint32_t> &bands,
                                     Error *error,
                                     const ResultCallback &callback,
                                     int timeout));
  MOCK_METHOD5(Command, void(const std::string &cmd,
                             const uint32_t &user_timeout,
                             Error *error,
                             const StringCallback &callback,
                             int timeout));
  MOCK_METHOD4(SetPowerState, void(const uint32_t &power_state,
                                   Error *error,
                                   const ResultCallback &callback,
                                   int timeout));
  MOCK_METHOD1(set_state_changed_callback, void(
      const ModemStateChangedSignalCallback &callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MOCK_MODEM_PROXY_H_
