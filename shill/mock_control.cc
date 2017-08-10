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

#include "shill/mock_control.h"

#include <base/memory/ptr_util.h>

#include "shill/mock_adaptors.h"

namespace shill {

MockControl::MockControl() {}

MockControl::~MockControl() {}

std::unique_ptr<DeviceAdaptorInterface> MockControl::CreateDeviceAdaptor(
    Device* /*device*/) {
  return base::MakeUnique<DeviceMockAdaptor>();
}

std::unique_ptr<IPConfigAdaptorInterface> MockControl::CreateIPConfigAdaptor(
    IPConfig* /*config*/) {
  return base::MakeUnique<IPConfigMockAdaptor>();
}

std::unique_ptr<ManagerAdaptorInterface> MockControl::CreateManagerAdaptor(
    Manager* /*manager*/) {
  return base::MakeUnique<ManagerMockAdaptor>();
}

std::unique_ptr<ProfileAdaptorInterface> MockControl::CreateProfileAdaptor(
    Profile* /*profile*/) {
  return base::MakeUnique<ProfileMockAdaptor>();
}

std::unique_ptr<RPCTaskAdaptorInterface> MockControl::CreateRPCTaskAdaptor(
    RPCTask* /*task*/) {
  return base::MakeUnique<RPCTaskMockAdaptor>();
}

std::unique_ptr<ServiceAdaptorInterface> MockControl::CreateServiceAdaptor(
    Service* /*service*/) {
  return base::MakeUnique<ServiceMockAdaptor>();
}

#ifndef DISABLE_VPN
std::unique_ptr<ThirdPartyVpnAdaptorInterface>
MockControl::CreateThirdPartyVpnAdaptor(ThirdPartyVpnDriver* /*driver*/) {
  return base::MakeUnique<ThirdPartyVpnMockAdaptor>();
}
#endif

const std::string& MockControl::NullRPCIdentifier() {
  return null_identifier_;
}

}  // namespace shill
