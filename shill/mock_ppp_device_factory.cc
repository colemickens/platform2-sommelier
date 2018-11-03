// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_ppp_device_factory.h"

namespace shill {

MockPPPDeviceFactory::MockPPPDeviceFactory() {}
MockPPPDeviceFactory::~MockPPPDeviceFactory() {}

MockPPPDeviceFactory* MockPPPDeviceFactory::GetInstance() {
  static base::NoDestructor<MockPPPDeviceFactory> instance;
  testing::Mock::AllowLeak(instance.get());
  return instance.get();
}

}  // namespace shill
