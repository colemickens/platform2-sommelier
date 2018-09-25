// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ppp_device_factory.h"

#include "shill/ppp_device.h"

using std::string;

namespace shill {

namespace {

base::LazyInstance<PPPDeviceFactory>::Leaky g_ppp_device_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

PPPDeviceFactory::PPPDeviceFactory() {}
PPPDeviceFactory::~PPPDeviceFactory() {}

PPPDeviceFactory* PPPDeviceFactory::GetInstance() {
  return g_ppp_device_factory.Pointer();
}

PPPDevice* PPPDeviceFactory::CreatePPPDevice(
    ControlInterface* control,
    EventDispatcher* dispatcher,
    Metrics* metrics,
    Manager* manager,
    const string& link_name,
    int interface_index) {
  return new PPPDevice(control, dispatcher, metrics, manager, link_name,
                       interface_index);
}

}  // namespace shill
