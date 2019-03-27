// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ppp_device_factory.h"

#include "shill/ppp_device.h"

namespace shill {

PPPDeviceFactory::PPPDeviceFactory() = default;
PPPDeviceFactory::~PPPDeviceFactory() = default;

PPPDeviceFactory* PPPDeviceFactory::GetInstance() {
  static base::NoDestructor<PPPDeviceFactory> instance;
  return instance.get();
}

PPPDevice* PPPDeviceFactory::CreatePPPDevice(Manager* manager,
                                             const std::string& link_name,
                                             int interface_index) {
  return new PPPDevice(manager, link_name, interface_index);
}

}  // namespace shill
