// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_MOCK_ETHERNET_SERVICE_H_
#define SHILL_ETHERNET_MOCK_ETHERNET_SERVICE_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/ethernet/ethernet_service.h"

namespace shill {

class MockEthernetService : public EthernetService {
 public:
  MockEthernetService(Manager* manager, base::WeakPtr<Ethernet> ethernet);
  ~MockEthernetService() override;

  MOCK_METHOD2(AddEAPCertification, bool(const std::string& name,
                                         size_t depth));
  MOCK_METHOD0(ClearEAPCertification, void());
  MOCK_METHOD2(Configure, void(const KeyValueStore& args, Error* error));
  MOCK_METHOD2(Disconnect, void(Error* error, const char* reason));
  MOCK_METHOD3(DisconnectWithFailure,
               void(ConnectFailure failure, Error* error, const char* reason));
  MOCK_CONST_METHOD1(GetDeviceRpcId, std::string(Error* error));
  MOCK_CONST_METHOD0(GetStorageIdentifier, std::string());
  MOCK_CONST_METHOD0(Is8021xConnectable, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
  MOCK_CONST_METHOD0(IsRemembered, bool());
  MOCK_METHOD1(SetFailure, void(ConnectFailure failure));
  MOCK_METHOD1(SetFailureSilent, void(ConnectFailure failure));
  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_METHOD0(OnVisibilityChanged, void());
  MOCK_CONST_METHOD0(technology, Technology::Identifier());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEthernetService);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_MOCK_ETHERNET_SERVICE_H_
