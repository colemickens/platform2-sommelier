// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_cellular.h"

namespace shill {

// TODO(rochberg): The cellular constructor does work.  Ought to fix
// this so that we don't depend on passing real values in for Type.

MockCellular::MockCellular(ControlInterface *control_interface,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager,
                           const std::string &link_name,
                           const std::string &address,
                           int interface_index,
                           Type type,
                           const std::string &owner,
                           const std::string &path,
                           mobile_provider_db *provider_db)
    : Cellular(control_interface, dispatcher, metrics, manager, link_name,
               address, interface_index, type, owner, path, provider_db) {}

MockCellular::~MockCellular() {}

}  // namespace shill
