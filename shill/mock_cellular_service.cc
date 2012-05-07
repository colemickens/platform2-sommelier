// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_cellular_service.h"

namespace shill {

MockCellularService::MockCellularService(ControlInterface *control_interface,
                                         EventDispatcher *dispatcher,
                                         Metrics *metrics,
                                         Manager *manager,
                                         const CellularRefPtr &device)
    : CellularService(control_interface, dispatcher, metrics, manager, device) {
}

MockCellularService::~MockCellularService() {}

}  // namespace shill
