//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/nice_mock_control.h"

#include <base/memory/ptr_util.h>
#include <gmock/gmock.h>

#include "shill/mock_adaptors.h"

using ::testing::NiceMock;

namespace shill {

NiceMockControl::NiceMockControl() {}

NiceMockControl::~NiceMockControl() {}

std::unique_ptr<DeviceAdaptorInterface> NiceMockControl::CreateDeviceAdaptor(
    Device* /*device*/) {
  return base::MakeUnique<NiceMock<DeviceMockAdaptor>>();
}

std::unique_ptr<IPConfigAdaptorInterface>
NiceMockControl::CreateIPConfigAdaptor(IPConfig* /*config*/) {
  return base::MakeUnique<NiceMock<IPConfigMockAdaptor>>();
}

std::unique_ptr<ManagerAdaptorInterface> NiceMockControl::CreateManagerAdaptor(
    Manager* /*manager*/) {
  return base::MakeUnique<NiceMock<ManagerMockAdaptor>>();
}

std::unique_ptr<ProfileAdaptorInterface> NiceMockControl::CreateProfileAdaptor(
    Profile* /*profile*/) {
  return base::MakeUnique<NiceMock<ProfileMockAdaptor>>();
}

std::unique_ptr<RPCTaskAdaptorInterface> NiceMockControl::CreateRPCTaskAdaptor(
    RPCTask* /*task*/) {
  return base::MakeUnique<NiceMock<RPCTaskMockAdaptor>>();
}

std::unique_ptr<ServiceAdaptorInterface> NiceMockControl::CreateServiceAdaptor(
    Service* /*service*/) {
  return base::MakeUnique<NiceMock<ServiceMockAdaptor>>();
}

#ifndef DISABLE_VPN
std::unique_ptr<ThirdPartyVpnAdaptorInterface>
NiceMockControl::CreateThirdPartyVpnAdaptor(ThirdPartyVpnDriver* /*driver*/) {
  return base::MakeUnique<NiceMock<ThirdPartyVpnMockAdaptor>>();
}
#endif

const std::string& NiceMockControl::NullRPCIdentifier() {
  return null_identifier_;
}

}  // namespace shill
