// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CELLULAR_H_
#define SHILL_MOCK_CELLULAR_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/cellular.h"

namespace shill {

class MockCellular : public Cellular {
 public:
  MockCellular(ModemInfo *modem_info,
               const std::string &link_name,
               const std::string &address,
               int interface_index,
               Type type,
               const std::string &owner,
               const std::string &service,
               const std::string &path,
               ProxyFactory *proxy_factory);
  virtual ~MockCellular();

  MOCK_METHOD1(Connect, void(Error *error));
  MOCK_METHOD1(Disconnect, void(Error *error));
  MOCK_METHOD3(OnDBusPropertiesChanged, void(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties));
  MOCK_METHOD1(set_modem_state, void(ModemState state));
  MOCK_METHOD0(DestroyService, void());
  MOCK_METHOD1(StartPPP, void(const std::string &serial_device));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCellular);
};

}  // namespace shill

#endif  // SHILL_MOCK_CELLULAR_H_
