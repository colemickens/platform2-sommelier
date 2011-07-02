// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <string>

#include <base/logging.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/shill_event.h"

using std::string;

namespace shill {
Profile::Profile(ControlInterface *control_interface)
    : adaptor_(control_interface->CreateProfileAdaptor(this)) {
}

Profile::~Profile() {}

}  // namespace shill
