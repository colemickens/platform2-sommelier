// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MOCK_MODEM_MODEMCDMA_PROXY_H_
#define SHILL_MM1_MOCK_MODEM_MODEMCDMA_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/mm1_modem_modemcdma_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockModemModemCdmaProxy : public ModemModemCdmaProxyInterface {
 public:
  MockModemModemCdmaProxy();
  virtual ~MockModemModemCdmaProxy();

  MOCK_METHOD4(Activate, void(
      const std::string &carrier,
      Error *error,
      const ResultCallback &callback,
      int timeout));

  MOCK_METHOD4(ActivateManual, void(
      const DBusPropertiesMap &properties,
      Error *error,
      const ResultCallback &callback,
      int timeout));

  MOCK_METHOD1(set_activation_state_callback,
               void(const ActivationStateSignalCallback &callback));

  // Properties.
  MOCK_METHOD0(Meid, std::string());
  MOCK_METHOD0(Esn, std::string());
  MOCK_METHOD0(Sid, uint32_t());
  MOCK_METHOD0(Nid, uint32_t());
  MOCK_METHOD0(Cdma1xRegistrationState, uint32_t());
  MOCK_METHOD0(EvdoRegistrationState, uint32_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemModemCdmaProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MOCK_MODEM_MODEMCDMA_PROXY_H_
