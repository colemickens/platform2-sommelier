// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"

namespace shill {

MockControl::MockControl() {}

MockControl::~MockControl() {}

ManagerAdaptorInterface *MockControl::CreateManagerAdaptor(Manager *manager) {
  return new ManagerMockAdaptor();
}

ServiceAdaptorInterface *MockControl::CreateServiceAdaptor(Service *service) {
  return new ServiceMockAdaptor();
}

DeviceAdaptorInterface *MockControl::CreateDeviceAdaptor(Device *device) {
  return new DeviceMockAdaptor();
}

ProfileAdaptorInterface *MockControl::CreateProfileAdaptor(Profile *profile) {
  return new ProfileMockAdaptor();
}

}  // namespace shill
