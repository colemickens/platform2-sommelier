// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_MOCK_VPN_DRIVER_H_
#define SHILL_VPN_MOCK_VPN_DRIVER_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/vpn/vpn_driver.h"

namespace shill {

class MockVPNDriver : public VPNDriver {
 public:
  MockVPNDriver();
  ~MockVPNDriver() override;

  MOCK_METHOD2(ClaimInterface,
               bool(const std::string& link_name, int interface_index));
  MOCK_METHOD2(Connect, void(const VPNServiceRefPtr& service, Error* error));
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD0(OnConnectionDisconnected, void());
  MOCK_METHOD2(Load,
               bool(StoreInterface* storage, const std::string& storage_id));
  MOCK_METHOD3(Save,
               bool(StoreInterface* storage,
                    const std::string& storage_id,
                    bool save_credentials));
  MOCK_METHOD0(UnloadCredentials, void());
  MOCK_METHOD1(InitPropertyStore, void(PropertyStore* store));
  MOCK_CONST_METHOD0(GetProviderType, std::string());
  MOCK_CONST_METHOD0(GetHost, std::string());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVPNDriver);
};

}  // namespace shill

#endif  // SHILL_VPN_MOCK_VPN_DRIVER_H_
