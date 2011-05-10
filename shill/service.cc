// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <string>

#include <base/logging.h>

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
    adaptor_(control_interface->CreateServiceAdaptor(this)) {
  // Initialize Interface montior, so we can detect new interfaces
  // TODO(cmasone): change this to VLOG(2) once we have it.
  LOG(INFO) << "Service initialized.";
}

Service::~Service() {
  delete(adaptor_);
}

}  // namespace shill
