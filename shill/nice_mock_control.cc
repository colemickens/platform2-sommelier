// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nice_mock_control.h"

#include <memory>

#include <gmock/gmock.h>

#include "shill/mock_adaptors.h"

using ::testing::NiceMock;

namespace shill {

NiceMockControl::NiceMockControl() = default;

NiceMockControl::~NiceMockControl() = default;

std::unique_ptr<DeviceAdaptorInterface> NiceMockControl::CreateDeviceAdaptor(
    Device* /*device*/) {
  return std::make_unique<NiceMock<DeviceMockAdaptor>>();
}

std::unique_ptr<IPConfigAdaptorInterface>
NiceMockControl::CreateIPConfigAdaptor(IPConfig* /*config*/) {
  return std::make_unique<NiceMock<IPConfigMockAdaptor>>();
}

std::unique_ptr<ManagerAdaptorInterface> NiceMockControl::CreateManagerAdaptor(
    Manager* /*manager*/) {
  return std::make_unique<NiceMock<ManagerMockAdaptor>>();
}

std::unique_ptr<ProfileAdaptorInterface> NiceMockControl::CreateProfileAdaptor(
    Profile* /*profile*/) {
  return std::make_unique<NiceMock<ProfileMockAdaptor>>();
}

std::unique_ptr<RPCTaskAdaptorInterface> NiceMockControl::CreateRPCTaskAdaptor(
    RPCTask* /*task*/) {
  return std::make_unique<NiceMock<RPCTaskMockAdaptor>>();
}

std::unique_ptr<ServiceAdaptorInterface> NiceMockControl::CreateServiceAdaptor(
    Service* /*service*/) {
  return std::make_unique<NiceMock<ServiceMockAdaptor>>();
}

#ifndef DISABLE_VPN
std::unique_ptr<ThirdPartyVpnAdaptorInterface>
NiceMockControl::CreateThirdPartyVpnAdaptor(ThirdPartyVpnDriver* /*driver*/) {
  return std::make_unique<NiceMock<ThirdPartyVpnMockAdaptor>>();
}
#endif

const std::string& NiceMockControl::NullRPCIdentifier() {
  return null_identifier_;
}

}  // namespace shill
