// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_wimax_service.h"

namespace shill {

MockWiMaxService::MockWiMaxService(ControlInterface *control,
                                   EventDispatcher *dispatcher,
                                   Metrics *metrics,
                                   Manager *manager,
                                   const WiMaxRefPtr &wimax)
    : WiMaxService(control, dispatcher, metrics, manager, wimax) {}

MockWiMaxService::~MockWiMaxService() {}

}  // namespace shill
