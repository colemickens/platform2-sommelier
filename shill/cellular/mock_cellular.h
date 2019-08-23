// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_CELLULAR_H_
#define SHILL_CELLULAR_MOCK_CELLULAR_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/cellular/cellular.h"

namespace shill {

class MockCellular : public Cellular {
 public:
  MockCellular(ModemInfo* modem_info,
               const std::string& link_name,
               const std::string& address,
               int interface_index,
               Type type,
               const std::string& service,
               const RpcIdentifier& path);
  ~MockCellular() override;

  MOCK_METHOD1(Connect, void(Error* error));
  MOCK_METHOD2(Disconnect, void(Error* error, const char* reason));
  MOCK_METHOD3(OnPropertiesChanged,
               void(const std::string& interface,
                    const KeyValueStore& changed_properties,
                    const std::vector<std::string>& invalidated_properties));
  MOCK_METHOD1(set_modem_state, void(ModemState state));
  MOCK_METHOD0(DestroyService, void());
  MOCK_METHOD1(StartPPP, void(const std::string& serial_device));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_CELLULAR_H_
