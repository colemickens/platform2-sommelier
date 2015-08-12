// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nice_mock_control.h"

#include <gmock/gmock.h>

#include "shill/mock_adaptors.h"

using ::testing::NiceMock;

namespace shill {

NiceMockControl::NiceMockControl() {}

NiceMockControl::~NiceMockControl() {}

DeviceAdaptorInterface* NiceMockControl::CreateDeviceAdaptor(
    Device* /*device*/) {
  return new NiceMock<DeviceMockAdaptor>();
}

IPConfigAdaptorInterface* NiceMockControl::CreateIPConfigAdaptor(
    IPConfig* /*config*/) {
  return new NiceMock<IPConfigMockAdaptor>();
}

ManagerAdaptorInterface* NiceMockControl::CreateManagerAdaptor(
    Manager* /*manager*/) {
  return new NiceMock<ManagerMockAdaptor>();
}

ProfileAdaptorInterface* NiceMockControl::CreateProfileAdaptor(
    Profile* /*profile*/) {
  return new NiceMock<ProfileMockAdaptor>();
}

RPCTaskAdaptorInterface* NiceMockControl::CreateRPCTaskAdaptor(
    RPCTask* /*task*/) {
  return new NiceMock<RPCTaskMockAdaptor>();
}

ServiceAdaptorInterface* NiceMockControl::CreateServiceAdaptor(
    Service* /*service*/) {
  return new NiceMock<ServiceMockAdaptor>();
}

#ifndef DISABLE_VPN
ThirdPartyVpnAdaptorInterface* NiceMockControl::CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver* /*driver*/) {
  return new NiceMock<ThirdPartyVpnMockAdaptor>();
}
#endif

const std::string& NiceMockControl::NullRPCIdentifier() {
  return null_identifier_;
}

}  // namespace shill
