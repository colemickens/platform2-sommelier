// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_control.h"

#include "shill/mock_adaptors.h"

namespace shill {

MockControl::MockControl() {}

MockControl::~MockControl() {}

DeviceAdaptorInterface* MockControl::CreateDeviceAdaptor(Device* /*device*/) {
  return new DeviceMockAdaptor();
}

IPConfigAdaptorInterface* MockControl::CreateIPConfigAdaptor(
    IPConfig* /*config*/) {
  return new IPConfigMockAdaptor();
}

ManagerAdaptorInterface* MockControl::CreateManagerAdaptor(
    Manager* /*manager*/) {
  return new ManagerMockAdaptor();
}

ProfileAdaptorInterface* MockControl::CreateProfileAdaptor(
    Profile* /*profile*/) {
  return new ProfileMockAdaptor();
}

RPCTaskAdaptorInterface* MockControl::CreateRPCTaskAdaptor(RPCTask* /*task*/) {
  return new RPCTaskMockAdaptor();
}

ServiceAdaptorInterface* MockControl::CreateServiceAdaptor(
    Service* /*service*/) {
  return new ServiceMockAdaptor();
}

#ifndef DISABLE_VPN
ThirdPartyVpnAdaptorInterface* MockControl::CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver* /*driver*/) {
  return new ThirdPartyVpnMockAdaptor();
}
#endif

const std::string& MockControl::NullRPCIdentifier() {
  return null_identifier_;
}

}  // namespace shill
