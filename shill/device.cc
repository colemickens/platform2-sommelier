// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <stdio.h>
#include <string>

#include "shill/shill_logging.h"
#include "shill/control_interface.h"
#include "shill/device.h"

using std::string;

namespace shill {
Device::Device(ControlInterface *control_interface,
		 EventDispatcher */* dispatcher */)
  : proxy_(control_interface->CreateDeviceProxy(this)),
    running_(false) {
  // Initialize Interface montior, so we can detect new interfaces
  SHILL_LOG(INFO, SHILL_LOG_DEVICE) << "Device initialized.";
}

Device::~Device() {
  delete(proxy_);
}

void Device::Start() {
  running_ = true;
  proxy_->UpdateEnabled();
}

void Device::Stop() {
  running_ = false;
  proxy_->UpdateEnabled();
}

}  // namespace shill
