// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_MOCK_ETHERNET_PROVIDER_H_
#define SHILL_ETHERNET_MOCK_ETHERNET_PROVIDER_H_

#include "shill/ethernet/ethernet_provider.h"

#include <gmock/gmock.h>

#include "shill/ethernet/ethernet_service.h"

namespace shill {

class MockEthernetProvider : public EthernetProvider {
 public:
  MockEthernetProvider();
  ~MockEthernetProvider() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(CreateService, EthernetServiceRefPtr(base::WeakPtr<Ethernet>));
  MOCK_METHOD2(GetService,
               ServiceRefPtr(const KeyValueStore& args, Error* error));
  MOCK_METHOD1(RegisterService, void(EthernetServiceRefPtr));
  MOCK_METHOD1(DeregisterService, void(EthernetServiceRefPtr));
  MOCK_METHOD0(RefreshGenericEthernetService, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEthernetProvider);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_MOCK_ETHERNET_PROVIDER_H_
