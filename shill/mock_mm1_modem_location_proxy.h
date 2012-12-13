// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MOCK_MODEM_LOCATION_PROXY_H_
#define SHILL_MM1_MOCK_MODEM_LOCATION_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/mm1_modem_location_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockModemLocationProxy : public ModemLocationProxyInterface {
 public:
  MockModemLocationProxy();
  virtual ~MockModemLocationProxy();

  // Inherited methods from ModemLocationProxyInterface.
  MOCK_METHOD5(Setup, void(uint32_t sources,
                           bool signal_location,
                           Error *error,
                           const ResultCallback &callback,
                           int timeout));

  MOCK_METHOD3(GetLocation, void(Error *error,
                                 const DBusEnumValueMapCallback &callback,
                                 int timeout));

  // Inherited properties from ModemLocationProxyInterface.
  MOCK_METHOD0(Capabilities, uint32_t());
  MOCK_METHOD0(Enabled, uint32_t());
  MOCK_METHOD0(SignalsLocation, bool());
  MOCK_METHOD0(Location, const DBusEnumValueMap());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemLocationProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MOCK_MODEM_LOCATION_PROXY_H_
