// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_service.h"

#include <string>

#include <base/memory/ref_counted.h>
#include <base/stringprintf.h>
#include <gmock/gmock.h>

#include "shill/refptr_types.h"

using std::string;

namespace shill {

class ControlInterface;
class EventDispatcher;

MockService::MockService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         const ProfileRefPtr &profile,
                         const string& name)
    : Service(control_interface, dispatcher, profile, name) {
}

MockService::~MockService() {}

}  // namespace shill
