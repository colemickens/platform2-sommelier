// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_manager.h"

namespace shill {

MockManager::MockManager(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         GLib *glib)
    : Manager(control_interface, dispatcher, glib, "", "", "") {}

MockManager::~MockManager() {}

}  // namespace shill
