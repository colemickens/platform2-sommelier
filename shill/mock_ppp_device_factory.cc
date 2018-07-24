// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_ppp_device_factory.h"

namespace shill {

namespace {

base::LazyInstance<MockPPPDeviceFactory> g_mock_ppp_device_factory
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

MockPPPDeviceFactory::MockPPPDeviceFactory() {}
MockPPPDeviceFactory::~MockPPPDeviceFactory() {}

MockPPPDeviceFactory* MockPPPDeviceFactory::GetInstance() {
  return g_mock_ppp_device_factory.Pointer();
}

}  // namespace shill
