// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DEVICE_INFO_
#define SHILL_MOCK_DEVICE_INFO_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/device_info.h"

namespace shill {

class ByteString;
class ControlInterface;
class EventDispatcher;
class Manager;

class MockDeviceInfo : public DeviceInfo {
 public:
  MockDeviceInfo(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 Manager *manager);
  virtual ~MockDeviceInfo();

  MOCK_CONST_METHOD2(GetMACAddress, bool(int, ByteString*));
  MOCK_CONST_METHOD2(GetFlags, bool(int, unsigned int*));
  MOCK_CONST_METHOD2(GetAddresses, bool(int, std::vector<AddressData>*));
  MOCK_CONST_METHOD1(FlushAddresses, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDeviceInfo);
};

}  // namespace shill

#endif  // SHILL_MOCK_DEVICE_INFO_
