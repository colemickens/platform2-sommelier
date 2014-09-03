// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MM1_MODEM_SIMPLE_PROXY_H_
#define SHILL_MOCK_MM1_MODEM_SIMPLE_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/mm1_modem_simple_proxy_interface.h"

namespace shill {
namespace mm1 {

class MockModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  MockModemSimpleProxy();
  ~MockModemSimpleProxy() override;

  MOCK_METHOD4(Connect, void(const DBusPropertiesMap &properties,
                             Error *error,
                             const DBusPathCallback &callback,
                             int timeout));
  MOCK_METHOD4(Disconnect, void(const ::DBus::Path &bearer,
                                Error *error,
                                const ResultCallback &callback,
                                int timeout));
  MOCK_METHOD3(GetStatus, void(Error *error,
                               const DBusPropertyMapCallback &callback,
                               int timeout));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockModemSimpleProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MOCK_MM1_MODEM_SIMPLE_PROXY_H_
