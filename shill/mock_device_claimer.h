// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DEVICE_CLAIMER_H_
#define SHILL_MOCK_DEVICE_CLAIMER_H_

#include "shill/device_claimer.h"

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockDeviceClaimer : public DeviceClaimer {
 public:
  explicit MockDeviceClaimer(const std::string& dbus_service_name);
  ~MockDeviceClaimer() override;

  MOCK_METHOD2(Claim, bool(const std::string& device_name, Error* error));
  MOCK_METHOD2(Release, bool(const std::string& device_name, Error* error));
  MOCK_METHOD0(DevicesClaimed, bool());
  MOCK_METHOD1(IsDeviceReleased, bool(const std::string& device_name));
  MOCK_CONST_METHOD0(default_claimer, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDeviceClaimer);
};

}  // namespace shill

#endif  // SHILL_MOCK_DEVICE_CLAIMER_H_
