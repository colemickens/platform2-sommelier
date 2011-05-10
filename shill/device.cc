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
  : adaptor_(control_interface->CreateDeviceAdaptor(this)),
    running_(false) {
  // Initialize Interface montior, so we can detect new interfaces
  SHILL_LOG(INFO, SHILL_LOG_DEVICE) << "Device initialized.";
}

Device::~Device() {
  delete(adaptor_);
}

void Device::Start() {
  running_ = true;
  adaptor_->UpdateEnabled();
}

void Device::Stop() {
  running_ = false;
  adaptor_->UpdateEnabled();
}

}  // namespace shill
