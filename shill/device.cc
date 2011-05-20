// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <string>

#include <base/logging.h>
#include <base/memory/ref_counted.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/shill_event.h"

using std::string;

namespace shill {
Device::Device(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               const string &link_name,
               int interface_index)
  : adaptor_(control_interface->CreateDeviceAdaptor(this)),
    link_name_(link_name),
    interface_index_(interface_index),
    running_(false) {
  // Initialize Interface monitor, so we can detect new interfaces
  VLOG(2) << "Device " << link_name_ << " index " << interface_index;
}

Device::~Device() {
  VLOG(2) << "Device " << link_name_ << " destroyed.";
  adaptor_.reset();
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
