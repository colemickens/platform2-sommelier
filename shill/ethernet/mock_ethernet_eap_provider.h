// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_MOCK_ETHERNET_EAP_PROVIDER_H_
#define SHILL_ETHERNET_MOCK_ETHERNET_EAP_PROVIDER_H_

#include "shill/ethernet/ethernet_eap_provider.h"

#include <gmock/gmock.h>

#include "shill/ethernet/ethernet_eap_service.h"

namespace shill {

class MockEthernetEapProvider : public EthernetEapProvider {
 public:
  MockEthernetEapProvider();
  ~MockEthernetEapProvider() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(SetCredentialChangeCallback,
               void(Ethernet* device, CredentialChangeCallback callback));
  MOCK_METHOD1(ClearCredentialChangeCallback, void(Ethernet* device));
  MOCK_CONST_METHOD0(OnCredentialsChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEthernetEapProvider);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_MOCK_ETHERNET_EAP_PROVIDER_H_
