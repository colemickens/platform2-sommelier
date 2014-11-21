// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_DEVICE_H_
#define APMANAGER_MOCK_DEVICE_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "apmanager/device.h"

namespace apmanager {

class MockDevice : public Device {
 public:
  MockDevice();
  ~MockDevice() override;

  MOCK_METHOD1(RegisterInterface,
               void(const WiFiInterface& interface));
  MOCK_METHOD1(DeregisterInterface,
               void(const WiFiInterface& interface));
  MOCK_METHOD1(ParseWiFiPhyInfo,
               void(const shill::Nl80211Message& msg));
  MOCK_METHOD0(ClaimDevice, bool());
  MOCK_METHOD0(ReleaseDevice, bool());
  MOCK_METHOD1(InterfaceExists, bool(const std::string& interface_name));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDevice);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_DEVICE_H_
