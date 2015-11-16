// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_manager.h"

namespace apmanager {

MockManager::MockManager(ControlInterface* control_interface)
    : Manager(control_interface) {}

MockManager::~MockManager() {}

}  // namespace apmanager
