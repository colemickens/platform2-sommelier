// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DEVICE_
#define SHILL_MOCK_DEVICE_

#include <string>

#include <gmock/gmock.h>

#include "shill/cellular.h"

namespace shill {

class MockCellular : public Cellular {
 public:
  MockCellular(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               const std::string &link_name,
               const std::string &address,
               int interface_index,
               Type type,
               const std::string &owner,
               const std::string &path,
               mobile_provider_db *provider_db);
  virtual ~MockCellular();

  MOCK_METHOD2(InitCapability, void(Type, ProxyFactory *));
  MOCK_METHOD1(OnModemManagerPropertiesChanged,
               void(const DBusPropertiesMap &));
  MOCK_METHOD1(set_modem_state, void(ModemState));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCellular);
};

}  // namespace shill

#endif  // SHILL_MOCK_CELLULAR_
