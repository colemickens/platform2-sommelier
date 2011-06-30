// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"

namespace shill {

MockControl::MockControl() {}

MockControl::~MockControl() {}

DeviceAdaptorInterface *MockControl::CreateDeviceAdaptor(Device *device) {
  return new DeviceMockAdaptor();
}

IPConfigAdaptorInterface *MockControl::CreateIPConfigAdaptor(IPConfig *config) {
  return new IPConfigMockAdaptor();
}

ManagerAdaptorInterface *MockControl::CreateManagerAdaptor(Manager *manager) {
  return new ManagerMockAdaptor();
}

ProfileAdaptorInterface *MockControl::CreateProfileAdaptor(Profile *profile) {
  return new ProfileMockAdaptor();
}

ServiceAdaptorInterface *MockControl::CreateServiceAdaptor(Service *service) {
  return new ServiceMockAdaptor();
}

}  // namespace shill
