// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <stdio.h>
#include <string>

#include "shill/shill_logging.h"
#include "shill/control_interface.h"
#include "shill/service.h"

using std::string;

namespace shill {
Service::Service(ControlInterface *control_interface,
		 EventDispatcher */* dispatcher */)
  : available_(false),
    configured_(false),
    auto_connect_(false),
    configuration_(NULL),
    connection_(NULL),
    proxy_(control_interface->CreateServiceProxy(this)) {
  // Initialize Interface montior, so we can detect new interfaces
  SHILL_LOG(INFO, SHILL_LOG_SERVICE) << "Service %p initialized.";
}

Service::~Service() {
  delete(proxy_);
}

}  // namespace shill
