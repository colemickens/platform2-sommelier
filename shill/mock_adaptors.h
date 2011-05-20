// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ADAPTORS_
#define SHILL_MOCK_ADAPTORS_

#include <string>

#include <gmock/gmock.h>

#include "shill/adaptor_interfaces.h"

namespace shill {

// These are the functions that a Manager adaptor must support
class ManagerMockAdaptor : public ManagerAdaptorInterface {
 public:
  ManagerMockAdaptor() {}
  virtual ~ManagerMockAdaptor() {}
  MOCK_METHOD0(UpdateRunning, void(void));
};

// These are the functions that a Service adaptor must support
class ServiceMockAdaptor : public ServiceAdaptorInterface {
 public:
  ServiceMockAdaptor() {}
  virtual ~ServiceMockAdaptor() {}
  MOCK_METHOD0(UpdateConnected, void(void));

};

// These are the functions that a Device adaptor must support
class DeviceMockAdaptor : public DeviceAdaptorInterface {
 public:
  DeviceMockAdaptor() {}
  virtual ~DeviceMockAdaptor() {}
  MOCK_METHOD0(UpdateEnabled, void(void));
};

}  // namespace shill
#endif  // SHILL_MOCK_ADAPTORS_
