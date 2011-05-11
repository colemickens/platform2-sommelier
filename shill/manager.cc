// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <string>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/manager.h"

using std::string;

namespace shill {
Manager::Manager(ControlInterface *control_interface,
                 EventDispatcher *dispatcher)
  : adaptor_(control_interface->CreateManagerAdaptor(this)),
    device_info_(dispatcher),
    running_(false) {
  // Initialize Interface monitor, so we can detect new interfaces
  // TODO(cmasone): change this to VLOG(2) once we have it.
  LOG(INFO) << "Manager initialized.";
}

Manager::~Manager() {}

void Manager::Start() {
  LOG(INFO) << "Manager started.";
  running_ = true;
  adaptor_->UpdateRunning();
  device_info_.Start();
}

void Manager::Stop() {
  running_ = false;
  adaptor_->UpdateRunning();
}

}  // namespace shill
