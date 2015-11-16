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
  explicit MockDevice(Manager* manager);
  ~MockDevice() override;

  MOCK_METHOD1(RegisterInterface,
               void(const WiFiInterface& interface));
  MOCK_METHOD1(DeregisterInterface,
               void(const WiFiInterface& interface));
  MOCK_METHOD1(ParseWiphyCapability,
               void(const shill::Nl80211Message& msg));
  MOCK_METHOD1(ClaimDevice, bool(bool full_control));
  MOCK_METHOD0(ReleaseDevice, bool());
  MOCK_METHOD1(InterfaceExists, bool(const std::string& interface_name));
  MOCK_METHOD2(GetHTCapability, bool(uint16_t channel, std::string* ht_capab));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDevice);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_DEVICE_H_
