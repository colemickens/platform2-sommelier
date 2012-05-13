// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/device.h"

#include <base/logging.h>

#include "wimax_manager/device_dbus_adaptor.h"

using std::string;

namespace wimax_manager {

Device::Device(uint8 index, const string &name)
    : index_(index), name_(name) {
}

Device::~Device() {
}

void Device::set_adaptor(DeviceDBusAdaptor *adaptor) {
  adaptor_.reset(adaptor);
}

}  // namespace wimax_manager
