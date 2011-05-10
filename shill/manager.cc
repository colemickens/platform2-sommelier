// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <stdio.h>
#include <string>

#include <glog/logging.h>
#include "shill/control_interface.h"
#include "shill/manager.h"

using std::string;

namespace shill {
Manager::Manager(ControlInterface *control_interface,
		 EventDispatcher */* dispatcher */)
  : adaptor_(control_interface->CreateManagerAdaptor(this)),
    running_(false) {
}

Manager::~Manager() {
  delete(adaptor_);
}

void Manager::Start() {
  running_ = true;
  adaptor_->UpdateRunning();
}

void Manager::Stop() {
  running_ = false;
  adaptor_->UpdateRunning();
}

}  // namespace shill
